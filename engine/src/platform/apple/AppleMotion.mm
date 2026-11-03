#import "platform/apple/AppleMotion.hpp"

#import <UIKit/UIKit.h>

#include <dlfcn.h>

#include <memory>
#include <utility>

#import "platform/MotionFrame.hpp"
#import "platform/NativeWindow.hpp"
#import "platform/apple/HaylenRequirement+Runtime.h"
#import "platform/apple/HaylenRequirements+Runtime.h"

// The vectors and the quaternion of Core Motion, `CMAcceleration`, `CMRotationRate` and `CMQuaternion`, laid out as the framework lays them out.
typedef struct {
    double x;
    double y;
    double z;
} HaylenMotionVector;

typedef struct {
    double x;
    double y;
    double z;
    double w;
} HaylenMotionQuaternion;

// The methods of `CMMotionManager` and of its readings that the engine calls, with `id` in place of the types of Core Motion, since the engine loads the framework at run time.
@protocol HaylenMotionManager <NSObject>

@property(assign, nonatomic) NSTimeInterval accelerometerUpdateInterval;
@property(assign, nonatomic) NSTimeInterval gyroUpdateInterval;
@property(assign, nonatomic) NSTimeInterval deviceMotionUpdateInterval;
@property(readonly, nonatomic, getter=isAccelerometerAvailable) BOOL accelerometerAvailable;
@property(readonly, nonatomic, getter=isGyroAvailable) BOOL gyroAvailable;
@property(readonly, nonatomic, getter=isDeviceMotionAvailable) BOOL deviceMotionAvailable;
@property(readonly, nullable) id accelerometerData;
@property(readonly, nullable) id gyroData;
@property(readonly, nullable) id deviceMotion;

+ (NSInteger)authorizationStatus;
- (void)startAccelerometerUpdates;
- (void)stopAccelerometerUpdates;
- (void)startGyroUpdates;
- (void)stopGyroUpdates;
- (void)startDeviceMotionUpdatesUsingReferenceFrame:(NSUInteger)referenceFrame;
- (void)startDeviceMotionUpdatesUsingReferenceFrame:(NSUInteger)referenceFrame toQueue:(NSOperationQueue*)queue withHandler:(void (^)(id _Nullable motion, NSError* _Nullable error))handler;
- (void)stopDeviceMotionUpdates;

@end

@protocol HaylenAccelerometerData <NSObject>
- (HaylenMotionVector)acceleration;
@end

@protocol HaylenGyroData <NSObject>
- (HaylenMotionVector)rotationRate;
@end

@protocol HaylenDeviceMotion <NSObject>
- (id)attitude;
@end

@protocol HaylenAttitude <NSObject>
- (HaylenMotionQuaternion)quaternion;
@end

namespace haylen::platform {

input::MotionSensors::Sensors AppleMotion::running;

id<HaylenMotionManager> AppleMotion::getManager() {
#if TARGET_OS_TV
    return nil;
#else
    // clang-format off
    static id<HaylenMotionManager> manager = []() -> id<HaylenMotionManager> {
        dlopen("/System/Library/Frameworks/CoreMotion.framework/CoreMotion", RTLD_LAZY);
        Class type = NSClassFromString(@"CMMotionManager");
        return type != nil ? (id<HaylenMotionManager>)[[type alloc] init] : nil;
    }();
    // clang-format on
    return manager;
#endif
}

bool AppleMotion::isGated() {
    id<HaylenMotionManager> manager = getManager();
    return manager != nil && [[manager class] respondsToSelector:@selector(authorizationStatus)];
}

input::MotionSensors::Support AppleMotion::getSupport() {
    input::MotionSensors::Support support;
#if TARGET_OS_TV
    support.reason = "An Apple TV has no motion sensors of its own. The Siri Remote and the controllers report their motion as gamepads, which the option \"gamepads\" reads.";
#else
    id<HaylenMotionManager> manager = getManager();
    support.sensors.set(static_cast<std::size_t>(input::MotionSensor::Accelerometer), manager.accelerometerAvailable);
    support.sensors.set(static_cast<std::size_t>(input::MotionSensor::Gyroscope), manager.gyroAvailable);
    support.sensors.set(static_cast<std::size_t>(input::MotionSensor::Attitude), manager.deviceMotionAvailable);
    if (support.sensors.none()) {
        support.reason = "This device has no motion sensors.";
    }
#endif
    return support;
}

Permission::State AppleMotion::getPermission() {
    if (!isGated()) {
        return Permission::State::Granted;
    }
    switch ([[getManager() class] authorizationStatus]) {
    case kStatusNotDetermined:
        return Permission::State::Prompt;
    case kStatusRestricted:
    case kStatusDenied:
        return Permission::State::Denied;
    default:
        return Permission::State::Granted;
    }
}

void AppleMotion::authorize(std::function<void(Permission::Answer answer)> callback) {
    const Permission::State state = getPermission();
    if (state != Permission::State::Prompt) {
        callback({.state = state, .message = state == Permission::State::Denied ? kDeniedMessage : ""});
        return;
    }

    NSError* error = nil;
    HaylenRequirements* requirements = [[HaylenRequirements alloc] initWithOwner:@"The motion sensors"];
    if (![requirements require:@[ [HaylenRequirement usageDescription:@"NSMotionUsageDescription"] ] error:&error]) {
        Permission::Answer answer{.state = Permission::State::Prompt, .message = [error.userInfo[@"message"] UTF8String]};
        for (NSDictionary<NSString*, NSString*>* entry in error.userInfo[@"data"][@"missing"]) {
            answer.missing.push_back({.kind = entry[@"kind"].UTF8String, .name = entry[@"name"].UTF8String, .file = entry[@"file"].UTF8String, .snippet = entry[@"snippet"].UTF8String});
        }
        callback(std::move(answer));
        return;
    }
    prompt(std::move(callback));
}

// The system asks the person when an app first starts the motion updates, and the first update or error comes once the person answered. No sensor runs before an app is allowed, so the updates stop there until a request runs them.
void AppleMotion::prompt(std::function<void(Permission::Answer answer)> callback) {
    id<HaylenMotionManager> manager = getManager();
    auto pending = std::make_shared<std::function<void(Permission::Answer)>>(std::move(callback));
    // clang-format off
    [manager startDeviceMotionUpdatesUsingReferenceFrame:kVerticalFrame toQueue:NSOperationQueue.mainQueue withHandler:^(id, NSError*) {
        if (!*pending) {
            return;
        }
        [manager stopDeviceMotionUpdates];
        const Permission::State state = getPermission();
        (*pending)({.state = state, .message = state == Permission::State::Denied ? kDeniedMessage : ""});
        *pending = nullptr;
    }];
    // clang-format on
}

void AppleMotion::set(input::MotionSensors::Sensors sensors, float rate) {
    id<HaylenMotionManager> manager = getManager();
    const NSTimeInterval interval = rate > 0.0F ? 1.0 / rate : 0.0;
    const auto wanted = [&sensors](input::MotionSensor sensor) { return sensors.test(static_cast<std::size_t>(sensor)); };
    const auto was = [](input::MotionSensor sensor) { return running.test(static_cast<std::size_t>(sensor)); };

    if (wanted(input::MotionSensor::Accelerometer)) {
        manager.accelerometerUpdateInterval = interval;
        if (!was(input::MotionSensor::Accelerometer)) {
            [manager startAccelerometerUpdates];
        }
    } else if (was(input::MotionSensor::Accelerometer)) {
        [manager stopAccelerometerUpdates];
    }

    if (wanted(input::MotionSensor::Gyroscope)) {
        manager.gyroUpdateInterval = interval;
        if (!was(input::MotionSensor::Gyroscope)) {
            [manager startGyroUpdates];
        }
    } else if (was(input::MotionSensor::Gyroscope)) {
        [manager stopGyroUpdates];
    }

    if (wanted(input::MotionSensor::Attitude)) {
        manager.deviceMotionUpdateInterval = interval;
        if (!was(input::MotionSensor::Attitude)) {
            [manager startDeviceMotionUpdatesUsingReferenceFrame:kVerticalFrame];
        }
    } else if (was(input::MotionSensor::Attitude)) {
        [manager stopDeviceMotionUpdates];
    }
    running = sensors;
}

int AppleMotion::getQuarterTurns() {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    switch (window.windowScene.interfaceOrientation) {
    case UIInterfaceOrientationLandscapeRight:
        return 1;
    case UIInterfaceOrientationPortraitUpsideDown:
        return 2;
    case UIInterfaceOrientationLandscapeLeft:
        return 3;
    default:
        return 0;
    }
#else
    return 0;
#endif
}

// Core Motion measures the pull of gravity in units of gravity, so a device that lies face up reads -1 on z, while the engine reads the push that holds the device up in meters per second squared.
void AppleMotion::read(input::MotionState& state) {
    id<HaylenMotionManager> manager = getManager();
    if (id<HaylenAccelerometerData> data = manager.accelerometerData; data != nil && running.test(static_cast<std::size_t>(input::MotionSensor::Accelerometer))) {
        const HaylenMotionVector value = data.acceleration;
        state.acceleration = {static_cast<float>(-value.x * kStandardGravity), static_cast<float>(-value.y * kStandardGravity), static_cast<float>(-value.z * kStandardGravity)};
        state.sensors.set(static_cast<std::size_t>(input::MotionSensor::Accelerometer));
    }
    if (id<HaylenGyroData> data = manager.gyroData; data != nil && running.test(static_cast<std::size_t>(input::MotionSensor::Gyroscope))) {
        const HaylenMotionVector value = data.rotationRate;
        state.rotationRate = {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
        state.sensors.set(static_cast<std::size_t>(input::MotionSensor::Gyroscope));
    }
    if (id<HaylenDeviceMotion> motion = manager.deviceMotion; motion != nil && running.test(static_cast<std::size_t>(input::MotionSensor::Attitude))) {
        const HaylenMotionQuaternion value = [(id<HaylenAttitude>)motion.attitude quaternion];
        state.attitude = {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z), static_cast<float>(value.w)};
        state.sensors.set(static_cast<std::size_t>(input::MotionSensor::Attitude));
    }
    MotionFrame::turn(state, getQuarterTurns());
}

} // namespace haylen::platform
