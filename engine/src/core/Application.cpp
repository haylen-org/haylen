#include "haylen/core/Application.hpp"

namespace haylen::core {

std::span<const std::string_view> Application::getSections() const {
    return {};
}

void Application::configure(AppConfig&) {}

void Application::addPlugins(Engine&) {}

void Application::stop(Engine&) {}

} // namespace haylen::core
