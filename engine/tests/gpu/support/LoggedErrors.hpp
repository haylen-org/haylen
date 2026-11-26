#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
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

    // Takes the first error that holds the text out of the errors and returns whether there was one, so a test that makes the device log an error checks it while every other error still fails the test.
    [[nodiscard]] bool take(std::string_view text);

  private:
    std::uint64_t listener = 0;
    std::mutex mutex;
    std::vector<std::string> errors;
};

} // namespace haylen::test
