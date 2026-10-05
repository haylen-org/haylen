-- The tests of the category in menu order.
return {
    prefix = 'NAT',
    title = 'Native libraries',
    description = 'The test library of the engine called through "haylen.native" and Varn "ffi", its callbacks and bridge handlers, a static library on iOS and tvOS and the handlers of each platform.',
    tests = {
        {code = 'NAT-001', title = 'Calls', description = 'Numbers, text, structs by value and by pointer, and buffers of the test library through Varn "ffi".', module = 'calls'},
        {code = 'NAT-002', title = 'Callbacks', description = 'Callbacks that run during the call, at the next frame and from a thread of the library.', module = 'callbacks'},
        {code = 'NAT-003', title = 'Library handlers', description = 'Handlers and events that the library registers through "HaylenNativeApi", with typed errors, timeouts and cancellation.', module = 'library-handlers'},
        {code = 'NAT-004', title = 'Static library', description = 'The library linked statically into an iOS or tvOS app and found through its symbol table.', module = 'static-library', platforms = {'ios', 'tvos'}, unsupported = {
            macos = 'Static libraries link into iOS and tvOS apps, and macOS loads the dynamic library.',
            windows = 'Static libraries link into iOS and tvOS apps, and Windows loads the dynamic library.',
            linux = 'Static libraries link into iOS and tvOS apps, and Linux loads the dynamic library.',
            android = 'Static libraries link into iOS and tvOS apps, and Android loads the dynamic library.',
            web = 'The browser links no native libraries.',
            headless = 'Static libraries link into iOS and tvOS apps, and the headless host loads the dynamic library.',
        }},
        {code = 'NAT-005', title = 'Platform handlers', description = 'Kotlin, Java, Swift and JavaScript handlers that answer, refuse, throw and stop when the app cancels.', module = 'platform-handlers', platforms = {'macos', 'ios', 'tvos', 'android', 'web'}, unsupported = {
            windows = 'The plugin "native-sample" has no native part for Windows.',
            linux = 'The plugin "native-sample" has no native part for Linux.',
            headless = 'The headless host loads no native parts of plugins.',
        }},
    },
}
