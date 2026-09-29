#import "platform/apple/AppleNetwork.hpp"

#import <Network/Network.h>

#include "haylen/platform/Event.hpp"
#include "platform/sokol/SokolRuntime.hpp"

namespace haylen::platform {

void AppleNetwork::observe() {
    static nw_path_monitor_t const monitor = nw_path_monitor_create();
    nw_path_monitor_set_queue(monitor, dispatch_get_main_queue());
    nw_path_monitor_set_update_handler(monitor, ^(nw_path_t path) { SokolRuntime::postEvent({.type = Event::Type::NetworkChanged, .online = nw_path_get_status(path) == nw_path_status_satisfied}); });
    nw_path_monitor_start(monitor);
}

} // namespace haylen::platform
