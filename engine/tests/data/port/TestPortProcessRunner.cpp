#include "varn/process/ProcessRunner.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace varn::process {

// The external driver of processes of the test port, which Varn compiles from the `VARN_DRIVER_SOURCES` of the port. The test port does not allow `processes`, so it runs no command, and it names its own folder so an app tells this driver from the empty one of Varn.
bool ProcessRunner::available() {
    return false;
}

ProcessResult ProcessRunner::exec(const std::string&, long long) {
    throw std::runtime_error("The test port starts no other program.");
}

std::optional<std::string> ProcessRunner::getenv(const std::string&) {
    return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> ProcessRunner::environment() {
    return {};
}

std::string ProcessRunner::cwd() {
    return "/test-port";
}

} // namespace varn::process
