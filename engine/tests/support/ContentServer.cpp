#include "support/ContentServer.hpp"

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerRequestImpl.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <format>
#include <fstream>
#include <thread>
#include <utility>

namespace haylen::test {

class ContentServer::Handler final : public Poco::Net::HTTPRequestHandler {
  public:
    explicit Handler(ContentServer& owner) : server(owner) {}

    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
        Poco::Net::StreamSocket& socket = static_cast<Poco::Net::HTTPServerRequestImpl&>(request).socket();
        server.serve(request.getURI(), request.get("Range", ""), request.get("If-Range", ""), response, socket);
    }

  private:
    ContentServer& server;
};

class ContentServer::Factory final : public Poco::Net::HTTPRequestHandlerFactory {
  public:
    explicit Factory(ContentServer& owner) : server(owner) {}

    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override {
        return new Handler(server);
    }

  private:
    ContentServer& server;
};

ContentServer::ContentServer(std::filesystem::path servedFolder) : folder(std::move(servedFolder)), listener(Poco::Net::SocketAddress("127.0.0.1", 0)) {
    auto* params = new Poco::Net::HTTPServerParams();
    params->setMaxThreads(8);
    server = std::make_unique<Poco::Net::HTTPServer>(new Factory(*this), listener, params);
    server->start();
}

ContentServer::~ContentServer() {
    stopping = true;
    server->stopAll(true);
}

std::string ContentServer::getUrl() const {
    return "http://127.0.0.1:" + std::to_string(listener.address().port()) + "/";
}

void ContentServer::touch(const std::string& path) {
    std::scoped_lock lock(mutex);
    ++versions[path];
}

void ContentServer::setStatus(const std::string& path, int status) {
    std::scoped_lock lock(mutex);
    statuses[path] = status;
}

void ContentServer::dropAfter(std::uint64_t bytes, int times) noexcept {
    dropBytes = bytes;
    drops = times;
}

std::vector<ContentServer::Exchange> ContentServer::getExchanges() const {
    std::scoped_lock lock(mutex);
    return exchanges;
}

std::uint64_t ContentServer::getSentBytes() const {
    std::scoped_lock lock(mutex);
    std::uint64_t total = 0;
    for (const Exchange& exchange : exchanges) {
        total += exchange.bytes;
    }
    return total;
}

void ContentServer::clearExchanges() {
    std::scoped_lock lock(mutex);
    exchanges.clear();
}

std::string ContentServer::getValidator(const std::string& path) {
    std::scoped_lock lock(mutex);
    return std::format("\"{}-{}\"", path.size(), versions[path]);
}

void ContentServer::record(Exchange exchange) {
    std::scoped_lock lock(mutex);
    exchanges.push_back(std::move(exchange));
}

void ContentServer::serve(const std::string& target, const std::string& range, const std::string& ifRange, Poco::Net::HTTPServerResponse& response, Poco::Net::StreamSocket& socket) {
    const std::string path = target.substr(1, target.find('?') == std::string::npos ? std::string::npos : target.find('?') - 1);
    Exchange exchange{.path = path};
    {
        std::scoped_lock lock(mutex);
        if (const auto found = statuses.find(path); found != statuses.end()) {
            exchange.status = found->second;
        }
    }
    const std::filesystem::path file = folder / path;
    std::error_code error;
    if (exchange.status == 0 && !std::filesystem::is_regular_file(file, error)) {
        exchange.status = 404;
    }
    if (exchange.status != 0) {
        response.setStatus(static_cast<Poco::Net::HTTPResponse::HTTPStatus>(exchange.status));
        response.setContentLength(0);
        response.send();
        record(std::move(exchange));
        return;
    }

    const std::uint64_t size = std::filesystem::file_size(file);
    const std::string validator = getValidator(path);
    response.set("ETag", validator);
    response.set("Accept-Ranges", "bytes");
    std::uint64_t first = 0;
    std::uint64_t last = size == 0 ? 0 : size - 1;
    bool partial = false;
    if (range.starts_with("bytes=") && !ignoresRanges && (ifRange.empty() || ifRange == validator)) {
        const std::string spec = range.substr(6);
        const std::size_t dash = spec.find('-');
        std::from_chars(spec.data(), spec.data() + dash, first);
        if (dash + 1 < spec.size()) {
            std::from_chars(spec.data() + dash + 1, spec.data() + spec.size(), last);
        }
        if (first >= size) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_REQUESTED_RANGE_NOT_SATISFIABLE);
            response.set("Content-Range", std::format("bytes */{}", size));
            response.setContentLength(0);
            response.send();
            exchange.status = 416;
            record(std::move(exchange));
            return;
        }
        last = std::min(last, size - 1);
        partial = true;
        exchange.first = first;
        exchange.last = last;
    }

    std::uint64_t sent = shiftsRanges && partial ? first + 1 : first;
    const std::uint64_t length = size == 0 ? 0 : last - sent + 1;
    exchange.status = partial ? 206 : 200;
    response.setStatus(partial ? Poco::Net::HTTPResponse::HTTP_PARTIAL_CONTENT : Poco::Net::HTTPResponse::HTTP_OK);
    if (partial) {
        response.set("Content-Range", std::format("bytes {}-{}/{}", sent, last, size));
    }
    response.setContentLength64(static_cast<Poco::Int64>(length));
    const bool drop = drops.load() > 0 && drops.fetch_sub(1) > 0;
    if (drop) {
        response.setKeepAlive(false);
    }
    std::ostream& output = response.send();

    for (int waited = 0; stalls && waited < kStallMilliseconds && !stopping; waited += 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::ifstream stream(file, std::ios::binary);
    stream.seekg(static_cast<std::streamoff>(sent));
    std::array<char, 64 * 1024> buffer{};
    const std::uint64_t limit = drop ? std::min(length, dropBytes.load()) : length;
    while (exchange.bytes < limit && output) {
        const auto count = static_cast<std::streamsize>(std::min<std::uint64_t>(buffer.size(), limit - exchange.bytes));
        stream.read(buffer.data(), count);
        output.write(buffer.data(), count);
        exchange.bytes += static_cast<std::uint64_t>(count);
    }
    output.flush();
    if (drop) {
        socket.shutdown();
    }
    record(std::move(exchange));
}

} // namespace haylen::test
