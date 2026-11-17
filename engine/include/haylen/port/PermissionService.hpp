#pragma once

#include <functional>

#include "haylen/platform/Permission.hpp"

namespace haylen::port {

// The permissions of the engine, `screens` and `localNetwork`, where the platform gates them behind a prompt or a policy. A port registers it where its platform gates one of them, and the engine answers `Granted` for every permission of a platform without it, since such a platform gates none. The callbacks run exactly once, on any thread and possibly after the engine that asked is gone.
class PermissionService {
  public:
    PermissionService() = default;
    PermissionService(const PermissionService&) = delete;
    PermissionService& operator=(const PermissionService&) = delete;
    virtual ~PermissionService() = default;

    // Reads the state of a permission without asking the person.
    virtual void checkPermission(platform::Permission::Kind kind, std::function<void(platform::Permission::Answer answer)> callback) = 0;

    // Asks the person for a permission with the prompt of the system, and answers with the state the person left.
    virtual void requestPermission(platform::Permission::Kind kind, std::function<void(platform::Permission::Answer answer)> callback) = 0;
};

} // namespace haylen::port
