-- The tests of the category in menu order.
return {
    prefix = 'PLT',
    title = 'Platform',
    description = 'The bridge to the native code of the plugin "platform-sample", what the window and the device report, "haylen.system", the native dialogs and every event of the platform.',
    tests = {
        {code = 'PLT-001', title = 'Native events', description = 'Events that the native code of the plugin "platform-sample" sends through the bridge, and the events of the platform itself.', module = 'native-events'},
        {code = 'PLT-002', title = 'Custom handler', description = 'The method "platform-sample.echo", which Java answers on Android, Objective-C on Apple platforms and JavaScript on the web.', module = 'custom-handler'},
        {code = 'PLT-003', title = 'Window and device', description = 'Platform, backend, safe area, orientation, pointer, fullscreen, the on-screen keyboard and the network.', module = 'window'},
        {code = 'PLT-004', title = 'System', description = 'What "haylen.system" tells about the device, its theme and battery with their changes, an address to open and a vibration.', module = 'system'},
        {code = 'PLT-005', title = 'Dialogs', description = 'A native message with three buttons, the pickers of files to open, of the destination of a save and of a folder, and a message that the app gives up.', module = 'dialogs'},
        {code = 'PLT-006', title = 'Event checklist', description = 'Every event that comes from the platform, marked live as it fires, and the events this platform never sends with the reason.', module = 'event-checklist'},
    },
}
