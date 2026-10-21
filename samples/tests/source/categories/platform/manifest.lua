-- The tests of the category in menu order.
return {
    prefix = 'PLT',
    title = 'Platform',
    description = 'The bridge to the native code of the plugin "platform-sample", what the window and the device report, "haylen.system", the native dialogs and every event of the platform.',
    tests = {
        {code = 'PLT-001', title = 'Native events', description = 'The ticks of a native ticker and an event sent from Lua through the plugin "platform-sample", next to the events of the platform.', module = 'native-events'},
        {code = 'PLT-002', title = 'Custom handler', description = 'The method "platform-sample.echo", answered by Java, Objective-C or JavaScript, and its failure without a text.', module = 'custom-handler'},
        {code = 'PLT-003', title = 'Window and device', description = 'Platform, graphics API, safe area, orientation, pointer, fullscreen, the on-screen keyboard and the network.', module = 'window'},
        {code = 'PLT-004', title = 'System', description = 'What "haylen.system" tells about the device, its theme and battery with their changes, an address to open and a vibration.', module = 'system'},
        {code = 'PLT-005', title = 'Dialogs', description = 'A native message, the pickers of files, of a place to save and of a folder, and a message that the app gives up.', module = 'dialogs'},
        {code = 'PLT-006', title = 'Event checklist', description = 'Every event that comes from the platform, marked live as it fires, and the events this platform never sends with the reason.', module = 'event-checklist'},
        {code = 'PLT-007', title = 'Launching apps', description = 'A small app package that the test writes into its storage and launches in place of the project, whose quit starts the project again.', module = 'launching'},
    },
}
