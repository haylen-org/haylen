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
