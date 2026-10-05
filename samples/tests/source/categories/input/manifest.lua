-- The tests of the category in menu order.
return {
    prefix = 'INP',
    title = 'Input',
    description = 'The keyboard, the mouse, touches, gestures, gamepads, the action map, remapping, touch controls and a hero that every device moves.',
    tests = {
        {code = 'INP-001', title = 'Keyboard', description = 'Keys held, pressed and released, modifiers, key events with repeats and the text typed with the layout of the platform.', module = 'keyboard'},
        {code = 'INP-002', title = 'Mouse', description = 'Buttons, the wheel, the movement of a frame, cursor shapes, a hidden cursor, a captured mouse and a finger acting as the mouse.', module = 'mouse', platforms = {'macos', 'windows', 'linux', 'ios', 'android', 'web', 'headless'}, unsupported = {tvos = 'Apple TV has no mouse or pointer, so there are no buttons, wheel, cursor or capture to show.'}},
        {code = 'INP-003', title = 'Touch', description = 'Up to ten fingers drawn with their id, phase, trail and time on the screen, and the mouse acting as a finger.', module = 'touch', platforms = {'macos', 'windows', 'linux', 'ios', 'android', 'web', 'headless'}, unsupported = {tvos = 'Apple TV has no touch screen, so no finger lands on the stage.'}},
        {code = 'INP-004', title = 'Gestures', description = 'Taps, double taps, long presses, swipes and pinches with their thresholds.', module = 'gestures', platforms = {'macos', 'windows', 'linux', 'ios', 'android', 'web', 'headless'}, unsupported = {tvos = 'Apple TV has neither a touch screen nor a mouse to make the gestures on the stage.'}},
        {code = 'INP-005', title = 'Gamepads', description = 'Four live controllers with sticks, triggers and buttons, their connections and the dead zone.', module = 'gamepads'},
        {code = 'INP-006', title = 'Action map', description = 'Button, axis and vector actions bound to keys, mouse, gamepads and touch controls, and the last device used.', module = 'actions'},
        {code = 'INP-007', title = 'Remapping', description = 'Rebinding the controls with key captures, kept in the preferences for the next launch.', module = 'remap'},
        {code = 'INP-008', title = 'Touch controls', description = 'A touch stick in its fixed, floating and following modes and touch buttons that drive virtual inputs of the action map.', module = 'virtual'},
        {code = 'INP-009', title = 'Character', description = 'A small platformer hero that every device moves at the same time, with prompts for the last device.', module = 'character'},
    },
}
