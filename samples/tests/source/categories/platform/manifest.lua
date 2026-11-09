-- The tests of the category in menu order.
return {
    prefix = 'PLT',
    title = 'Platform',
    description = 'The bridge to the native code of the plugin "platform-sample", what the window and the device report, "haylen.system", every event of the platform, the displays, the outputs on other displays, the permissions and the frame rate.',
    tests = {
        {code = 'PLT-001', title = 'Native events', description = 'The ticks of a native ticker and an event sent from Lua through the plugin "platform-sample", next to the events of the platform.', module = 'native-events'},
        {code = 'PLT-002', title = 'Custom handler', description = 'The method "platform-sample.echo", answered by Java, Objective-C or JavaScript, and its failure without a text.', module = 'custom-handler'},
        {code = 'PLT-003', title = 'Window and device', description = 'Platform, graphics API, safe area, orientation, pointer, fullscreen, keeping the screen on, the on-screen keyboard and the network.', module = 'window'},
        {code = 'PLT-004', title = 'System', description = 'What "haylen.system" tells about the device, its theme, battery, power saving and time zone with their changes, and a copy to the clipboard and a read of it.', module = 'system'},
        {code = 'PLT-006', title = 'Event checklist', description = 'Every event that comes from the platform, marked live as it fires, and the events this platform never sends with the reason.', module = 'event-checklist'},
        {code = 'PLT-007', title = 'Launching apps', description = 'A small app package that the test writes into its storage and launches in place of the project, whose quit starts the project again.', module = 'launching'},
        {code = 'PLT-008', title = 'Displays', description = 'Every display where it sits with its id, scale, rate and modes, the window moved to another display, fullscreen on it and in its lowest mode, and the events of the displays.', module = 'displays'},
        {code = 'PLT-009', title = 'Second display', description = 'A scoreboard on a display other than the one of the window, drawn into the target of an output every frame, with its preview, its events and the clicks on its window.', module = 'second-display'},
        {code = 'PLT-010', title = 'Permissions', description = 'The state of every permission of the engine on this platform, a request for each one, and the error of a clipboard that the person refused.', module = 'permissions'},
        {code = 'PLT-011', title = 'Frame rate', description = 'The app aiming for every refresh of the display or for 120, 60, 30 or 24 frames per second, with the frames it played in the last second, a ball that swings by the real time and a mark for each frame.', module = 'frame-rate'},
    },
}
