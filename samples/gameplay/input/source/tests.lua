-- The tests of the sample in menu order. Each module returns the scene class of its test.
return {
    {id = 'keyboard', title = 'Keyboard', description = 'Keys held, pressed and released, modifiers, key events with repeats and the text typed with the layout of the platform.', module = 'tests.keyboard'},
    {id = 'mouse', title = 'Mouse', description = 'Buttons, the wheel, the movement of a frame, cursor shapes, a hidden cursor and a captured mouse.', module = 'tests.mouse'},
    {id = 'touch', title = 'Touch', description = 'Up to ten fingers drawn with their id, phase, trail and time on the screen.', module = 'tests.touch'},
    {id = 'gestures', title = 'Gestures', description = 'Taps, double taps, long presses, swipes and pinches with their thresholds.', module = 'tests.gestures'},
    {id = 'gamepads', title = 'Gamepads', description = 'Four live controllers with sticks, triggers and buttons, their connections and the dead zone.', module = 'tests.gamepads'},
    {id = 'actions', title = 'Action map', description = 'Button, axis and vector actions bound to keys, mouse, gamepads and touch controls, and the last device used.', module = 'tests.actions'},
    {id = 'remap', title = 'Remapping', description = 'Rebinding the controls with key captures, kept in the preferences for the next launch.', module = 'tests.remap'},
    {id = 'virtual', title = 'Touch controls', description = 'A touch stick and touch buttons that drive virtual inputs of the action map.', module = 'tests.virtual'},
    {id = 'character', title = 'Character', description = 'A small platformer hero that every device moves at the same time, with prompts for the last device.', module = 'tests.character'},
}
