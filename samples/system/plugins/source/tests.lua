-- The tests of the sample in menu order. Each module returns the scene class of its test.
return {
    {id = 'calls', title = 'Calls', description = 'The call "echo" on the main thread, "compute" on a background thread, a typed failure, and a call that only a timeout or a cancel ends.', module = 'tests.calls'},
    {id = 'events', title = 'Events', description = 'The "tick" events of a native timer, and "loaded", which the native part sent retained when it loaded.', module = 'tests.events'},
    {id = 'bytes', title = 'Binary payloads', description = 'Bytes that cross to the native part and back as buffers, and an image that the native part draws and returns as PNG bytes.', module = 'tests.bytes'},
    {id = 'streams', title = 'Streams', description = 'A video stream that the native part draws 30 times per second, and an audio stream of a tone that the app plays.', module = 'tests.streams'},
    {id = 'batches', title = 'Batched events', description = '100 events per frame from the native part, which arrive as one list per frame.', module = 'tests.batches'},
    {id = 'configuration', title = 'Configuration', description = 'The parameters of the plugin from "app.json" and the defaults of "plugin.json", in Lua and in the native part.', module = 'tests.configuration'},
    {id = 'banner', title = 'Native banner', description = 'A native view over the app at the top or bottom that may reserve its edge, with a native button, while other taps reach the app.', module = 'tests.banner'},
    {id = 'cover', title = 'Covering native UI', description = 'A native screen over the whole app, which halts the app until it closes.', module = 'tests.cover'},
    {id = 'screen', title = 'Native screen', description = 'A screen of the plugin, a UIKit, AppKit or SwiftUI screen on Apple platforms, an AndroidX activity on Android, a popup page on the web and a native window on the desktops, whose answer reaches the call, and after a restart the next app as "screenRestored".', module = 'tests.screen'},
    {id = 'result', title = 'Native result', description = 'The file picker of the platform, which answers with the name of the picked file.', module = 'tests.result'},
    {id = 'permissions', title = 'Permissions', description = 'The camera and notification prompts of the system, and a local notification whose tap reaches the app, even from a closed app.', module = 'tests.permissions'},
    {id = 'requirements', title = 'Requirements', description = 'A call whose native part needs what the project of the app lacks, which fails with the code "unsupported" and lists what is missing and how to add it.', module = 'tests.requirements'},
    {id = 'urls', title = 'Opened URLs', description = 'Links with the scheme of the plugin that open the app, before or after it started.', module = 'tests.urls'},
    {id = 'errors', title = 'App errors', description = 'An error of the app that the native part keeps and sends back to the next app.', module = 'tests.errors'},
    {id = 'info', title = 'Plugin info', description = 'The plugins of the app and whether their native part runs on this platform.', module = 'tests.info'},
}
