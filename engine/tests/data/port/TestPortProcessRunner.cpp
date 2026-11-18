#include "varn/process/ProcessRunner.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace varn::process {

// The external driver of processes of the test port, which Varn compiles from the `VARN_DRIVER_SOURCES` of the port. The test port does not allow `processes`, so it runs no command and has no environment, and it names its own folder so an app tells this driver from the dummy driver of Varn.
bool ProcessRunner::available() {
    return false;
}

ProcessResult ProcessRunner::exec(const std::string&, long long) {
    throw std::runtime_error("The test port starts no other program.");
}

std::optional<std::string> ProcessRunner::getenv(const std::string&) {
    throw std::runtime_error("The test port has no environment of a process.");
}

std::vector<std::pair<std::string, std::string>> ProcessRunner::environment() {
    throw std::runtime_error("The test port has no environment of a process.");
}

std::string ProcessRunner::cwd() {
    return "/test-port";
}

} // namespace varn::process
