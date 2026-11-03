#pragma once

#import <Foundation/Foundation.h>

#include <functional>

#include "haylen/input/MotionSensors.hpp"
#include "haylen/input/MotionState.hpp"
#include "haylen/platform/Permission.hpp"

@protocol HaylenMotionManager;

namespace haylen::platform {

// The motion sensors of an iPhone, an iPad or a Vision Pro through Core Motion, which the engine loads the first time the app asks for them instead of linking it, so apps that never read them link nothing of it. From version 27.2 the system asks the person for them with the `NSMotionUsageDescription` of the `Info.plist`. An Apple TV has no such sensors, since the Siri Remote and the controllers report their motion as gamepads, and neither does a Mac that runs a Mac Catalyst app.
class AppleMotion final {
  public:
    [[nodiscard]] static input::MotionSensors::Support getSupport();

    // Asks the person for the motion sensors where the system gates them, after it checked the `NSMotionUsageDescription` that the prompt shows, as the permission `motion` of `Permission` does.
    static void authorize(std::function<void(Permission::Answer answer)> callback);
    static void set(input::MotionSensors::Sensors sensors, float rate);
    static void read(input::MotionState& state);

    // The state of the permission `motion` of `Permission`.
    [[nodiscard]] static Permission::State getPermission();

  private:
    static constexpr double kStandardGravity = 9.80665;

    // The values of `CMAuthorizationStatus`, and the reference frame `CMAttitudeReferenceFrameXArbitraryZVertical`, whose z points up.
    static constexpr NSInteger kStatusNotDetermined = 0;
    static constexpr NSInteger kStatusRestricted = 1;
    static constexpr NSInteger kStatusDenied = 2;
    static constexpr NSUInteger kVerticalFrame = 1;

    // The manager of Core Motion, one per process as Core Motion asks, or nil where the system has no Core Motion.
    [[nodiscard]] static id<HaylenMotionManager> getManager();

    // Whether the system asks the person for the motion sensors, which it does from version 27.2.
    [[nodiscard]] static bool isGated();

    // The quarter turns counterclockwise from portrait that the interface of the window shows in.
    [[nodiscard]] static int getQuarterTurns();

    // Runs the motion updates once, which shows the prompt of the system, and answers with the state the person left.
    static void prompt(std::function<void(Permission::Answer answer)> callback);

    static input::MotionSensors::Sensors running;
};

} // namespace haylen::platform
