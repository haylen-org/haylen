-- The tests of the sample in menu order. Each module returns the scene class of its test.
return {
    {id = 'calls', title = 'Calls', description = 'echo on the main thread, compute on a background thread, a typed failure, and a call that only a timeout or a cancel ends.', module = 'tests.calls'},
    {id = 'events', title = 'Events', description = 'tick events of a native timer, and loaded, which the native part sent retained when it loaded.', module = 'tests.events'},
    {id = 'configuration', title = 'Configuration', description = 'The parameters of the plugin from app.json and the defaults of plugin.json, in Lua and in the native part.', module = 'tests.configuration'},
    {id = 'banner', title = 'Native banner', description = 'A native view over the app at the top or bottom that may reserve its edge, with a native button, while other taps reach the app.', module = 'tests.banner'},
    {id = 'cover', title = 'Covering native UI', description = 'A native screen over the whole app, which halts the app until it closes.', module = 'tests.cover'},
    {id = 'result', title = 'Native result', description = 'The file picker of the platform, which answers with the name of the picked file.', module = 'tests.result'},
    {id = 'urls', title = 'Opened URLs', description = 'Links with the scheme of the plugin that open the app, before or after it started.', module = 'tests.urls'},
    {id = 'errors', title = 'App errors', description = 'An error of the app that the native part keeps and sends back to the next app.', module = 'tests.errors'},
    {id = 'info', title = 'Plugin info', description = 'The plugins of the app and whether their native part runs on this platform.', module = 'tests.info'},
}
