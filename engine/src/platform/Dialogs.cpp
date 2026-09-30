#include "haylen/platform/Dialogs.hpp"

#include <format>
#include <stdexcept>
#include <string>
#include <system_error>

#include "haylen/core/Log.hpp"
#include "platform/Host.hpp"

namespace haylen::platform {

std::atomic<std::uint64_t> Dialogs::nextId{1};

// Picked files that an earlier app copied are gone once a new app starts, so the folder never grows.
Dialogs::Dialogs(Host& owner, std::filesystem::path root) : host(owner), folder(std::move(root)) {
    std::error_code error;
    std::filesystem::remove_all(folder, error);
    if (error) {
        core::Log::warning("The copies of picked files in \"{}\" could not be removed: {}.", folder.string(), error.message());
    }
}

Dialogs::~Dialogs() {
    for (const auto& [id, entry] : pending) {
        host.cancelDialog(id);
    }
}

std::uint64_t Dialogs::show(const DialogRequest& request, Callback callback, std::optional<std::chrono::steady_clock::duration> timeout) {
    validate(request);
    if (timeout && timeout->count() <= 0) {
        throw std::invalid_argument("The timeout of a dialog is a positive duration.");
    }

    // The platform may answer while it shows the dialog, and the answer waits for the next pump like any other.
    const std::uint64_t id = nextId.fetch_add(1);
    Pending entry{.callback = std::move(callback)};
    if (timeout) {
        entry.deadline = std::chrono::steady_clock::now() + *timeout;
    }
    host.showDialog(id, request, folder / std::to_string(id));
    pending.emplace(id, std::move(entry));
    return id;
}

bool Dialogs::cancel(std::uint64_t id) {
    const auto found = pending.find(id);
    if (found == pending.end()) {
        return false;
    }
    Callback callback = std::move(found->second.callback);
    pending.erase(found);

    host.cancelDialog(id);
    cancelled.emplace_back(std::move(callback), DialogResult{.failure = DialogResult::Failure{.code = DialogResult::Code::Cancelled, .message = "The dialog was cancelled."}});
    return true;
}

void Dialogs::resolve(std::uint64_t id, DialogResult result) {
    const std::scoped_lock lock(mutex);
    answers.push_back({.id = id, .result = std::move(result)});
}

void Dialogs::pump() {
    std::vector<Answer> arrived;
    {
        const std::scoped_lock lock(mutex);
        arrived.swap(answers);
    }

    // An answer to a dialog this engine never showed, or that already settled, belongs to nobody and is dropped.
    for (Answer& answer : arrived) {
        const auto found = pending.find(answer.id);
        if (found == pending.end()) {
            continue;
        }
        Callback callback = std::move(found->second.callback);
        pending.erase(found);
        if (callback) {
            callback(std::move(answer.result));
        }
    }

    for (auto& [callback, result] : std::exchange(cancelled, {})) {
        if (callback) {
            callback(std::move(result));
        }
    }
    expire();
}

std::size_t Dialogs::getPendingCount() const noexcept {
    return pending.size() + cancelled.size();
}

void Dialogs::expire() {
    const auto now = std::chrono::steady_clock::now();
    std::vector<std::uint64_t> overdue;
    for (const auto& [id, entry] : pending) {
        if (entry.deadline && *entry.deadline <= now) {
            overdue.push_back(id);
        }
    }

    for (const std::uint64_t id : overdue) {
        const auto found = pending.find(id);
        if (found == pending.end()) {
            continue;
        }
        Callback callback = std::move(found->second.callback);
        pending.erase(found);
        host.cancelDialog(id);
        if (callback) {
            callback({.failure = DialogResult::Failure{.code = DialogResult::Code::Timeout, .message = "The dialog timed out."}});
        }
    }
}

void Dialogs::validate(const DialogRequest& request) {
    if (const auto* message = std::get_if<DialogRequest::Message>(&request.dialog)) {
        if (message->text.empty()) {
            throw std::invalid_argument("A message dialog needs a text.");
        }
        if (message->buttons.empty() || message->buttons.size() > 3) {
            throw std::invalid_argument(std::format("A message dialog has one to three buttons, not {}.", message->buttons.size()));
        }
        for (const std::string& label : message->buttons) {
            if (label.empty()) {
                throw std::invalid_argument("Every button of a message dialog needs a label.");
            }
        }
        return;
    }
    if (const auto* openFiles = std::get_if<DialogRequest::OpenFiles>(&request.dialog)) {
        validateFilters(openFiles->filters);
        return;
    }
    if (const auto* saveFile = std::get_if<DialogRequest::SaveFile>(&request.dialog)) {
        validateFilters(saveFile->filters);
        if (saveFile->name.empty()) {
            throw std::invalid_argument("A save dialog needs the name it suggests for the file.");
        }
        if (saveFile->name.find_first_of("/\\") != std::string::npos) {
            throw std::invalid_argument(std::format("The name \"{}\" that a save dialog suggests is a file name, without folders.", saveFile->name));
        }
    }
}

// Extensions reach every platform in the same form, without the dot and without the wildcards and separators of the filter syntaxes of each platform.
void Dialogs::validateFilters(const std::vector<DialogRequest::Filter>& filters) {
    for (const DialogRequest::Filter& filter : filters) {
        if (filter.name.empty()) {
            throw std::invalid_argument("Every filter of a file dialog needs a name.");
        }
        if (filter.extensions.empty()) {
            throw std::invalid_argument(std::format("The filter \"{}\" needs at least one extension.", filter.name));
        }
        for (const std::string& extension : filter.extensions) {
            if (extension.empty() || extension.front() == '.' || extension.find_first_of("*?/\\;, \t") != std::string::npos) {
                throw std::invalid_argument(std::format("The extension \"{}\" of the filter \"{}\" is invalid. Extensions come without their dot, such as \"png\" or \"tar.gz\".", extension, filter.name));
            }
        }
    }
}

} // namespace haylen::platform
