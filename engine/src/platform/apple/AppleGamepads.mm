#import "platform/apple/AppleGamepads.hpp"

#include <vector>

#import "platform/apple/AppleRemote.hpp"

namespace haylen::platform {

GamepadSlots AppleGamepads::slots;

// GameController lists the connected controllers in no lasting order, so every controller stays in the slot it took when it connected.
void AppleGamepads::poll(std::span<input::GamepadState> states) {
    NSArray<GCController*>* controllers = GCController.controllers;
    std::vector<const void*> connected;
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
