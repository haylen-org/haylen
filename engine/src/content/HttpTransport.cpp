#include "content/HttpTransport.hpp"

#include "varn/http/HttpClient.h"

#include <algorithm>
#include <format>
#include <limits>
#include <mutex>
#include <utility>

namespace haylen::content {

namespace client = varn::http::client;

struct HttpTransport::Transfer::State {
    Request request;
    Callbacks callbacks;
    std::mutex mutex;
    client::Handle handle;
    bool cancelled = false;

    // The request keeps its state alive until it completes, and the fields below change only on the thread that polls the loop.
    std::uint64_t received = 0;
    bool restarting = false;
    std::optional<Error> failure;
};

void HttpTransport::Transfer::cancel() const {
    if (!state) {
        return;
    }
    std::scoped_lock lock(state->mutex);
    state->cancelled = true;
    state->handle.cancel();
}

bool HttpTransport::Transfer::isActive() const {
    if (!state) {
        return false;
    }
    std::scoped_lock lock(state->mutex);
    return state->handle.active();
}

HttpTransport::HttpTransport(varn::runtime::Runtime& runtime) : client(std::make_unique<client::HttpClient>(runtime)) {}

// A request that never completes, such as one that the app cancels as it stops, keeps its state and the transfer of the client alive through each other, so the transport lets both go when it ends.
HttpTransport::~HttpTransport() {
    for (const std::weak_ptr<Transfer::State>& weak : states) {
        if (const std::shared_ptr<Transfer::State> state = weak.lock()) {
            const std::scoped_lock lock(state->mutex);
            state->handle.cancel();
            state->handle = {};
            state->callbacks = {};
        }
    }
}

std::chrono::milliseconds HttpTransport::getTimeout(std::uint64_t bytes) noexcept {
    return kBaseTimeout + std::chrono::milliseconds(bytes * 1000 / kSlowestBytesPerSecond);
}

HttpTransport::Transfer HttpTransport::fetch(Request request, Callbacks callbacks) {
    auto state = std::make_shared<Transfer::State>();
    state->request = std::move(request);
    state->callbacks = std::move(callbacks);
    std::erase_if(states, [](const std::weak_ptr<Transfer::State>& weak) { return weak.expired(); });
    states.push_back(state);
    send(state);
    return Transfer(state);
}

void HttpTransport::send(const std::shared_ptr<Transfer::State>& state) {
    // Only a validator guarantees that the bytes held come from the same file, so a range without one starts again.
    if (state->request.validator.empty()) {
        state->request.held = 0;
    }
    const Request& request = state->request;
    client::Request http;
    http.url = request.url;
    std::uint64_t expected = request.maximumSize;
    if (request.size) {
        const std::uint64_t first = request.offset + request.held;
        expected = *request.size - request.held;
        http.headers.emplace_back("Range", std::format("bytes={}-{}", first, request.offset + *request.size - 1));
        if (request.held > 0) {
            http.headers.emplace_back("If-Range", request.validator);
        }
    }
    // A range takes no limit from the client, which would refuse a whole file before its head arrives, so its head reports a server that sends the whole file, and the bytes are counted here instead.
    http.maxResponseBytes = request.size ? std::numeric_limits<std::uint64_t>::max() : expected;
    http.timeout = request.timeout.value_or(getTimeout(expected));
    state->received = 0;

    client::HttpClient::Callbacks callbacks;
    // clang-format off
    callbacks.onHead = [state](const client::ResponseHead& head) {
        const std::optional<client::ContentRange> range = head.contentRange();
        std::optional<std::uint64_t> fileSize = range ? range->completeLength : std::nullopt;
        if (!state->request.size && head.status == 200) {
            fileSize = head.contentLength;
        }
        receiveHead(*state, head.status, range ? std::optional(range->first) : std::nullopt, range ? std::optional(range->last) : std::nullopt, fileSize, readValidator(head.header("etag")));
    };
    callbacks.onChunk = [state](std::string_view chunk) {
        receiveBytes(*state, {reinterpret_cast<const std::uint8_t*>(chunk.data()), chunk.size()});
    };
    callbacks.onComplete = [this, state](const std::optional<client::Error>& error) {
        if (state->failure) {
            complete(state, std::move(state->failure));
            return;
        }
        if (state->restarting) {
            state->restarting = false;
            send(state);
            return;
        }
        complete(state, error ? std::optional(describeFailure(*state, static_cast<int>(error->code), error->message)) : std::nullopt);
    };
    // clang-format on

    std::scoped_lock lock(state->mutex);
    if (state->cancelled) {
        state->failure = Error(Error::Code::NetworkCancelled, "The download of \"" + request.url + "\" was cancelled.");
    }
    state->handle = client->send(std::move(http), std::move(callbacks));
    if (state->cancelled) {
        state->handle.cancel();
    }
}

void HttpTransport::receiveHead(Transfer::State& state, int status, std::optional<std::uint64_t> first, std::optional<std::uint64_t> last, std::optional<std::uint64_t> fileSize, std::string validator) {
    const Request& request = state.request;
    if (!request.size) {
        if (status != 200) {
            stop(state, Error(Error::Code::UnexpectedHttpStatus, std::format("The server answered \"{}\" with the status {} instead of the file.", request.url, status), {.httpStatus = status}));
            return;
        }
        state.callbacks.onStart({.validator = std::move(validator), .fileSize = fileSize});
        return;
    }

    // A server answers a range it does not serve, or a resumed range of a file that changed, with the whole file, which is never read: a changed file starts its range again, and a server without ranges stops the download.
    if (status == 200 && request.held > 0) {
        state.restarting = true;
        state.request.held = 0;
        state.request.validator.clear();
        stop(state, std::nullopt);
        return;
    }
    if (status == 200) {
        stop(state, Error(Error::Code::RangeUnsupported, std::format("The server of \"{}\" answered a byte range with the whole file, and protected content needs a server that serves byte ranges.", request.url), {.httpStatus = status}));
        return;
    }
    if (status != 206) {
        stop(state, Error(Error::Code::UnexpectedHttpStatus, std::format("The server answered the range of \"{}\" with the status {}.", request.url, status), {.httpStatus = status}));
        return;
    }

    const std::uint64_t expectedFirst = request.offset + request.held;
    const std::uint64_t expectedLast = request.offset + *request.size - 1;
    if (first != expectedFirst || last != expectedLast) {
        stop(state, Error(Error::Code::RangeUnsupported, std::format("The server answered the bytes {} to {} of \"{}\" with another range, and protected content needs a server that serves byte ranges exactly.", expectedFirst, expectedLast, request.url), {.httpStatus = status}));
        return;
    }
    state.callbacks.onStart({.resumed = request.held > 0, .validator = std::move(validator), .fileSize = fileSize});
}

void HttpTransport::receiveBytes(Transfer::State& state, std::span<const std::uint8_t> bytes) {
    if (state.failure || state.restarting) {
        return;
    }
    state.received += bytes.size();
    if (state.request.size && state.received > *state.request.size - state.request.held) {
        stop(state, Error(Error::Code::RangeUnsupported, std::format("The server sent more than the {} bytes asked for of \"{}\".", *state.request.size - state.request.held, state.request.url)));
        return;
    }
    try {
        state.callbacks.onBytes(bytes);
    } catch (const Error& error) {
        stop(state, error);
    } catch (const std::exception& error) {
        stop(state, Error(Error::Code::NetworkFailed, "The download of \"" + state.request.url + "\" stopped: " + error.what()));
    }
}

void HttpTransport::stop(Transfer::State& state, std::optional<Error> failure) {
    state.failure = std::move(failure);
    std::scoped_lock lock(state.mutex);
    state.handle.cancel();
}

void HttpTransport::complete(const std::shared_ptr<Transfer::State>& state, std::optional<Error> failure) {
    const Request& request = state->request;
    if (!failure && request.size && state->received != *request.size - request.held) {
        failure = Error(Error::Code::UpdateInterrupted, std::format("The download of \"{}\" ended after {} of {} bytes.", request.url, state->received, *request.size - request.held));
    }
    const std::function<void(std::optional<Error>)> onComplete = std::move(state->callbacks.onComplete);
    state->callbacks = {};
    onComplete(std::move(failure));
}

Error HttpTransport::describeFailure(const Transfer::State& state, int code, const std::string& message) {
    const std::string& url = state.request.url;
    const Error::Details details{.platformMessage = message};
    switch (static_cast<client::ErrorCode>(code)) {
    case client::ErrorCode::Network:
        if (state.received > 0) {
            return {Error::Code::UpdateInterrupted, "The connection to the server ended during the download of \"" + url + "\": " + message, details};
        }
        return {Error::Code::NetworkOffline, "The server of \"" + url + "\" could not be reached: " + message, details};
    case client::ErrorCode::Timeout:
        return {Error::Code::NetworkTimeout, "The download of \"" + url + "\" took too long: " + message, details};
    case client::ErrorCode::Cancelled:
        return {Error::Code::NetworkCancelled, "The download of \"" + url + "\" was cancelled.", details};
    case client::ErrorCode::Tls:
        return {Error::Code::TlsFailed, "The secure connection to the server of \"" + url + "\" failed: " + message, details};
    default:
        return {Error::Code::NetworkFailed, "The download of \"" + url + "\" failed: " + message, details};
    }
}

std::string HttpTransport::readValidator(std::optional<std::string_view> etag) {
    // Only a strong entity tag names exact bytes, so a weak one never resumes a download.
    if (!etag || etag->starts_with("W/") || etag->size() < 2 || etag->front() != '"' || etag->back() != '"') {
        return {};
    }
    return std::string(*etag);
}

} // namespace haylen::content
