#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace haylen::test {

// Keeps the errors that the log receives while it lives, from any thread, such as the messages of the validation of a graphics API that a GPU device logs, so a test fails on them.
class LoggedErrors final {
  public:
    LoggedErrors();
    LoggedErrors(const LoggedErrors&) = delete;
    LoggedErrors& operator=(const LoggedErrors&) = delete;
    ~LoggedErrors();

    // Fails the running test once for each error the log received.
    void reportAsFailures();

  private:
    std::uint64_t listener = 0;
    std::mutex mutex;
    std::vector<std::string> errors;
};

} // namespace haylen::test
