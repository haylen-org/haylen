#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/platform/DialogRequest.hpp"
#include "haylen/platform/DialogResult.hpp"

namespace haylen::platform {

class Host;

// Shows native dialogs and hands their answers to the frame thread. Each callback runs once, in `pump`, with the choice of the user or with a failure: `cancelled` when the app gave the dialog up, `timeout` when its time ran out, `unsupported` where the platform has no such dialog and `failed` where the platform could not show it. Dialog ids are unique in the whole process, so an answer that arrives after the app restarted never answers a dialog of the new app.
class Dialogs final {
  public:
    using Callback = std::function<void(DialogResult)>;

    // The platform copies picked files that have no path of their own, such as the documents of mobile pickers, into a folder of each dialog under `folder`, which every new engine empties.
    Dialogs(Host& host, std::filesystem::path folder);

    // Closes the dialogs that are still open, whose callbacks never run.
    ~Dialogs();

    Dialogs(const Dialogs&) = delete;
    Dialogs& operator=(const Dialogs&) = delete;

    // Validates the request, hands it to the platform and returns its id. A dialog with a timeout fails with the code `timeout` when the user takes longer, and the platform closes it. Throws `std::invalid_argument` for a request the platform cannot show.
    std::uint64_t show(const DialogRequest& request, Callback callback, std::optional<std::chrono::steady_clock::duration> timeout = std::nullopt);

    // Closes a dialog, which fails with the code `cancelled` at the next pump, and returns `false` when it already settled.
    bool cancel(std::uint64_t id);

    // Answers a dialog from any thread. An answer to a dialog that settled or that this engine never showed is dropped.
    void resolve(std::uint64_t id, DialogResult result);

    void pump();
    [[nodiscard]] std::size_t getPendingCount() const noexcept;

  private:
    struct Pending {
        Callback callback;
        std::optional<std::chrono::steady_clock::time_point> deadline;
    };

    struct Answer {
        std::uint64_t id = 0;
        DialogResult result;
    };

    static std::atomic<std::uint64_t> nextId;

    static void validate(const DialogRequest& request);
    static void validateFilters(const std::vector<DialogRequest::Filter>& filters);

    // Gives up the dialogs whose timeout passed and closes them.
    void expire();

    Host& host;
    std::filesystem::path folder;
    std::unordered_map<std::uint64_t, Pending> pending;
    std::vector<std::pair<Callback, DialogResult>> cancelled;
    std::mutex mutex;
    std::vector<Answer> answers;
};

} // namespace haylen::platform
