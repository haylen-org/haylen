#include "haylen/core/Application.hpp"
#include "haylen/lua/Application.hpp"

namespace haylen::core {

std::unique_ptr<Application> Application::create() {
    return std::make_unique<lua::Application>();
}

} // namespace haylen::core
