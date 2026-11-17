#pragma once

#include <memory>

#include "haylen/port/AccessibilityService.hpp"
#include "haylen/port/AudioOutput.hpp"
#include "haylen/port/BridgeService.hpp"
#include "haylen/port/DesktopService.hpp"
#include "haylen/port/DisplayService.hpp"
#include "haylen/port/FontService.hpp"
#include "haylen/port/LinkService.hpp"
#include "haylen/port/MediaService.hpp"
#include "haylen/port/NetworkService.hpp"
#include "haylen/port/PermissionService.hpp"
#include "haylen/port/PlatformService.hpp"
#include "haylen/port/ScreenService.hpp"
#include "haylen/port/VideoService.hpp"

namespace haylen::port {

// The services of a platform by capability, which its runtime registers once, before the player starts, and which live as long as the process. The platform and its displays are required, and every other service is registered only where the platform has that capability, so the engine answers in one place for a capability that a platform lacks, as each service describes, and an empty implementation never reads as one that worked. One object may serve several capabilities, so the fields share it.
struct Services {
    std::shared_ptr<PlatformService> platform;
    std::shared_ptr<DisplayService> displays;
    std::shared_ptr<DesktopService> desktop;
    std::shared_ptr<ScreenService> screen;
    std::shared_ptr<BridgeService> bridge;
    std::shared_ptr<PermissionService> permissions;
    std::shared_ptr<NetworkService> network;
    std::shared_ptr<LinkService> links;
    std::shared_ptr<MediaService> media;
    std::shared_ptr<AccessibilityService> accessibility;
    std::shared_ptr<VideoService> video;
    std::shared_ptr<FontService> fonts;
    std::shared_ptr<AudioOutput> audio;
};

} // namespace haylen::port
