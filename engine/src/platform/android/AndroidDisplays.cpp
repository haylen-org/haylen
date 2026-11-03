#include "platform/android/AndroidDisplays.hpp"

#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/Player.hpp"

namespace haylen::platform {

std::mutex& AndroidDisplays::mutex = *new std::mutex();
std::vector<Display>& AndroidDisplays::displays = *new std::vector<Display>();
std::string& AndroidDisplays::windowDisplayId = *new std::string();

void AndroidDisplays::receive(std::string_view json, int windowDisplay) {
    const core::Json list = core::Json::parse(json);
    std::vector<Display> received;
    received.reserve(list.size());
    for (const core::Json& item : list) {
        const core::Json& work = item.at("workArea");
        Display& display = received.emplace_back(Display{
            .id = item.at("id").get<std::string>(),
            .name = item.at("name").get<std::string>(),
            .bounds = {0.0F, 0.0F, item.at("width").get<float>(), item.at("height").get<float>()},
            .workArea = {work[0].get<float>(), work[1].get<float>(), work[2].get<float>(), work[3].get<float>()},
            .scale = item.at("scale").get<float>(),
            .refreshRate = item.at("refreshRate").get<float>(),
            .primary = item.at("primary").get<bool>(),
        });
        for (const core::Json& mode : item.at("modes")) {
            display.modes.push_back({.width = mode[0].get<int>(), .height = mode[1].get<int>(), .refreshRate = mode[2].get<float>(), .scale = display.scale});
        }
    }

    {
        const std::scoped_lock lock(mutex);
        displays = std::move(received);
        windowDisplayId = std::to_string(windowDisplay);
    }
    Player::postEvent({.type = Event::Type::DisplaysChanged});
}

std::vector<Display> AndroidDisplays::getDisplays() {
    const std::scoped_lock lock(mutex);
    return displays;
}

std::string AndroidDisplays::getWindowDisplayId() {
    const std::scoped_lock lock(mutex);
    return windowDisplayId;
}

} // namespace haylen::platform
