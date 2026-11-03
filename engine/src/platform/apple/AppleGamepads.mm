#import "platform/apple/AppleGamepads.hpp"

#import <objc/runtime.h>

#include <algorithm>
#include <vector>

#import "platform/apple/AppleRemote.hpp"

// The methods of CoreHaptics that a rumble calls, with `id` in place of its types, since the engine reaches the framework through GameController without linking it.
@protocol HaylenHapticEngine <NSObject>

- (BOOL)startAndReturnError:(NSError**)error;
- (id)createPlayerWithPattern:(id)pattern error:(NSError**)error;

@end

@protocol HaylenHapticPattern <NSObject>

- (id)initWithDictionary:(NSDictionary*)pattern error:(NSError**)error;

@end

@protocol HaylenHapticPlayer <NSObject>

- (BOOL)startAtTime:(NSTimeInterval)time error:(NSError**)error;
- (BOOL)stopAtTime:(NSTimeInterval)time error:(NSError**)error;

@end

namespace haylen::platform {

GamepadSlots AppleGamepads::slots;
std::vector<const void*> AppleGamepads::connected;
input::MotionSensors::Sensors AppleGamepads::motion;
char AppleGamepads::engineKey = 0;
char AppleGamepads::playerKey = 0;

// GameController lists the connected controllers in no lasting order, so every controller stays in the slot it took when it connected. The list of the connected ones keeps its memory from frame to frame.
void AppleGamepads::poll(std::span<input::GamepadState> states) {
    NSArray<GCController*>* controllers = GCController.controllers;
    connected.clear();
    for (GCController* controller in controllers) {
        if (controller.extendedGamepad != nil || controller.microGamepad != nil) {
            connected.push_back((__bridge const void*)controller);
        }
    }
    slots.update(connected);

    for (std::size_t index = 0; index < states.size(); ++index) {
        input::GamepadState& state = states[index];
        GCController* controller = find(controllers, slots.getController(index));
        if (controller == nil) {
            state = {};
            continue;
        }

        state.connected = true;
        NSString* vendor = controller.vendorName;
        state.name = vendor != nil ? vendor.UTF8String : "Controller";
        readMotion(controller, state);
        readTouchpads(controller, state);
        if (controller.extendedGamepad == nil) {
            state.buttons = {};
            state.axes = {};
            AppleRemote::read(controller.microGamepad, state);
            continue;
        }
        read(controller.extendedGamepad, state);
        state.labels = getLabels(controller.extendedGamepad);
    }
}

// The symbol of the bottom face button names the label printed on it.
input::GamepadLabels AppleGamepads::getLabels(GCExtendedGamepad* pad) {
    NSString* symbol = pad.buttonA.sfSymbolsName;
    if ([symbol hasPrefix:@"a."]) {
        return input::GamepadLabels::Letters;
    }
    if ([symbol hasPrefix:@"b."]) {
        return input::GamepadLabels::SwappedLetters;
    }
    if ([symbol hasPrefix:@"xmark"]) {
        return input::GamepadLabels::Shapes;
    }
    return input::GamepadLabels::Unknown;
}

void AppleGamepads::rumble(std::size_t slot, float low, float high, float seconds) {
    GCController* controller = find(GCController.controllers, slots.getController(slot));
    Class patternClass = NSClassFromString(@"CHHapticPattern");
    if (controller == nil || controller.haptics == nil || patternClass == nil) {
        return;
    }
    id<HaylenHapticPlayer> previous = objc_getAssociatedObject(controller, &playerKey);
    [previous stopAtTime:0 error:nil];
    objc_setAssociatedObject(controller, &playerKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    const float intensity = std::max(low, high);
    if (intensity <= 0.0F) {
        return;
    }

    id<HaylenHapticEngine> engine = objc_getAssociatedObject(controller, &engineKey);
    if (engine == nil) {
        engine = (id<HaylenHapticEngine>)[controller.haptics createEngineWithLocality:GCHapticsLocalityDefault];
        if (engine == nil) {
            return;
        }
        objc_setAssociatedObject(controller, &engineKey, engine, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    NSDictionary* event = @{
        @"EventType" : @"HapticContinuous",
        @"Time" : @0,
        @"EventDuration" : @(seconds),
        @"EventParameters" : @[ @{@"ParameterID" : @"HapticIntensity",
                                  @"ParameterValue" : @(intensity)},
                                @{@"ParameterID" : @"HapticSharpness",
                                  @"ParameterValue" : @(high / (low + high))} ],
    };
    id<HaylenHapticPattern> pattern = [(id<HaylenHapticPattern>)[patternClass alloc] initWithDictionary:@{@"Pattern" : @[ @{@"Event" : event} ]} error:nil];
    if (pattern == nil || ![engine startAndReturnError:nil]) {
        return;
    }
    id<HaylenHapticPlayer> player = [engine createPlayerWithPattern:pattern error:nil];
    if ([player startAtTime:0 error:nil]) {
        objc_setAssociatedObject(controller, &playerKey, player, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
}

void AppleGamepads::setMotion(input::MotionSensors::Sensors sensors) {
    motion = sensors;
}

// GameController measures the pull of gravity in units of gravity on the axes of the controller, while the engine reads the push that holds it up in meters per second squared. A controller whose sensors wait for the app runs them only while the app asks, and the Siri Remote runs its own.
void AppleGamepads::readMotion(GCController* controller, input::GamepadState& state) {
    state.motionSensors.reset();
    state.motion = {};
    GCMotion* sensors = controller.motion;
    if (sensors == nil) {
        return;
    }
    state.motionSensors.set(static_cast<std::size_t>(input::MotionSensor::Accelerometer));
    state.motionSensors.set(static_cast<std::size_t>(input::MotionSensor::Gyroscope), sensors.hasRotationRate);
    state.motionSensors.set(static_cast<std::size_t>(input::MotionSensor::Attitude), sensors.hasAttitude);
    const bool wanted = motion.any();
    if (sensors.sensorsRequireManualActivation && sensors.sensorsActive != wanted) {
        sensors.sensorsActive = wanted;
    }
    if (!wanted || (sensors.sensorsRequireManualActivation && !sensors.sensorsActive)) {
        return;
    }

    const auto take = [&state](input::MotionSensor sensor) { return state.motionSensors.test(static_cast<std::size_t>(sensor)) && motion.test(static_cast<std::size_t>(sensor)); };
    if (take(input::MotionSensor::Accelerometer)) {
        const GCAcceleration value = sensors.acceleration;
        state.motion.acceleration = {static_cast<float>(-value.x * kStandardGravity), static_cast<float>(-value.y * kStandardGravity), static_cast<float>(-value.z * kStandardGravity)};
        state.motion.sensors.set(static_cast<std::size_t>(input::MotionSensor::Accelerometer));
    }
    if (take(input::MotionSensor::Gyroscope)) {
        const GCRotationRate value = sensors.rotationRate;
        state.motion.rotationRate = {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
        state.motion.sensors.set(static_cast<std::size_t>(input::MotionSensor::Gyroscope));
    }
    if (take(input::MotionSensor::Attitude)) {
        const GCQuaternion value = sensors.attitude;
        state.motion.attitude = {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z), static_cast<float>(value.w)};
        state.motion.sensors.set(static_cast<std::size_t>(input::MotionSensor::Attitude));
    }
}

// The touchpads of the profile of a controller tell whether a finger touches them, and their surface runs from -1 to 1 with up positive, while the engine reads them from 0 to 1 from the top left corner. The touchpad of the Siri Remote is its stick instead.
void AppleGamepads::readTouchpads(GCController* controller, input::GamepadState& state) {
    state.touchpad = {};
    if (controller.extendedGamepad == nil) {
        return;
    }
    // The profile keeps its touchpads by name in no lasting order, so the names in order give every touchpad its slot.
    NSDictionary<NSString*, GCControllerTouchpad*>* touchpads = controller.physicalInputProfile.touchpads;
    std::size_t slot = 0;
    for (NSString* name in [touchpads.allKeys sortedArrayUsingSelector:@selector(compare:)]) {
        if (slot == state.touchpad.size()) {
            return;
        }
        GCControllerTouchpad* touchpad = touchpads[name];
        input::TouchpadFinger& finger = state.touchpad[slot];
        finger.touchpad = static_cast<std::uint8_t>(slot++);
        finger.down = touchpad.touchState != GCTouchStateUp;
        finger.position = {(touchpad.touchSurface.xAxis.value + 1.0F) / 2.0F, (1.0F - touchpad.touchSurface.yAxis.value) / 2.0F};
        finger.pressure = finger.down ? 1.0F : 0.0F;
    }
}

GCController* AppleGamepads::find(NSArray<GCController*>* controllers, const void* identity) {
    for (GCController* controller in controllers) {
        if ((__bridge const void*)controller == identity) {
            return controller;
        }
    }
    return nil;
}

void AppleGamepads::read(GCExtendedGamepad* pad, input::GamepadState& state) {
    const auto set = [&state](input::GamepadButton button, GCControllerButtonInput* control) { state.buttons[static_cast<std::size_t>(button)] = control != nil && control.pressed; };
    set(input::GamepadButton::South, pad.buttonA);
    set(input::GamepadButton::East, pad.buttonB);
    set(input::GamepadButton::West, pad.buttonX);
    set(input::GamepadButton::North, pad.buttonY);
    set(input::GamepadButton::LeftShoulder, pad.leftShoulder);
    set(input::GamepadButton::RightShoulder, pad.rightShoulder);
    set(input::GamepadButton::Back, pad.buttonOptions);
    set(input::GamepadButton::Start, pad.buttonMenu);
    set(input::GamepadButton::Guide, pad.buttonHome);
    set(input::GamepadButton::LeftStick, pad.leftThumbstickButton);
    set(input::GamepadButton::RightStick, pad.rightThumbstickButton);
    set(input::GamepadButton::DpadUp, pad.dpad.up);
    set(input::GamepadButton::DpadDown, pad.dpad.down);
    set(input::GamepadButton::DpadLeft, pad.dpad.left);
    set(input::GamepadButton::DpadRight, pad.dpad.right);

    // GameController reports up as positive, while the engine follows the screen with down as positive.
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = pad.leftThumbstick.xAxis.value;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = -pad.leftThumbstick.yAxis.value;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightX)] = pad.rightThumbstick.xAxis.value;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightY)] = -pad.rightThumbstick.yAxis.value;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftTrigger)] = pad.leftTrigger.value;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)] = pad.rightTrigger.value;
}

} // namespace haylen::platform
