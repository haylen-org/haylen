-- The tests of the category in menu order.
return {
    prefix = 'PLT',
    title = 'Platform',
    description = 'The bridge to the native code of the plugin "platform-sample", what the window and the device report, "haylen.system", the native dialogs and every event of the platform.',
    tests = {
        {code = 'PLT-001', title = 'Native events', description = 'Checks that the native code of the plugin "platform-sample" sends the ticks of its ticker from the language of the platform and that an event sent from Lua arrives, failing where the project builds the native code and it is missing, next to the events of the platform itself.', module = 'native-events'},
        {code = 'PLT-002', title = 'Custom handler', description = 'Checks the answer of the method "platform-sample.echo", which Java gives on Android, Objective-C on Apple platforms and JavaScript on the web, and its failure without a text, failing where the project builds the native code and it is missing.', module = 'custom-handler'},
        {code = 'PLT-003', title = 'Window and device', description = 'Platform, graphics API, safe area, orientation, pointer, fullscreen, the on-screen keyboard and the network.', module = 'window'},
        {code = 'PLT-004', title = 'System', description = 'What "haylen.system" tells about the device, its theme and battery with their changes, an address to open and a vibration.', module = 'system'},
        {code = 'PLT-005', title = 'Dialogs', description = 'A native message with three buttons, the pickers of files to open, of the destination of a save and of a folder, and a message that the app gives up.', module = 'dialogs'},
        {code = 'PLT-006', title = 'Event checklist', description = 'Every event that comes from the platform, marked live as it fires, and the events this platform never sends with the reason.', module = 'event-checklist'},
        {code = 'PLT-007', title = 'Launching apps', description = 'A small app package that the test writes into its storage and launches in place of the project with a new engine, whose Back button, quit or back button at its root starts the project again.', module = 'launching'},
    },
}
