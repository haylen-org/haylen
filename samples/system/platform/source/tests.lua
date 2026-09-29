-- The tests of the sample in menu order. Each module returns the scene class of its test.
return {
    {id = 'built-ins', title = 'Built-in methods', description = 'engine.info, app.version, device.info, system.locale, system.openUrl and haptics.vibrate with their answers.', module = 'tests.built-ins'},
    {id = 'native-events', title = 'Native events', description = 'Events that the native code of this app sends through the bridge, and the events of the platform itself.', module = 'tests.native-events'},
    {id = 'custom-handler', title = 'Custom handler', description = 'sample.echo, answered by Java on Android, Objective-C on Apple platforms and JavaScript on the web.', module = 'tests.custom-handler'},
    {id = 'window', title = 'Window and device', description = 'Platform, backend, safe area, orientation, pointer, fullscreen, the on-screen keyboard and the network.', module = 'tests.window'},
}
