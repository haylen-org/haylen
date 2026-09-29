-- The tests of the sample in menu order. Each module returns the scene class of its test.
return {
    {id = 'calls', title = 'Calls', description = 'Numbers, text, structs by value and by pointer, and buffers of the test library through Varn ffi.', module = 'tests.calls'},
    {id = 'callbacks', title = 'Callbacks', description = 'Callbacks that run during the call, at the next frame and from a thread of the library.', module = 'tests.callbacks'},
    {id = 'library-handlers', title = 'Library handlers', description = 'Handlers and events that the library registers through HaylenNativeApi, with typed errors, timeouts and cancellation.', module = 'tests.library-handlers'},
    {id = 'static-library', title = 'Static library', description = 'The library linked statically into an iOS or tvOS app and found through its symbol table.', module = 'tests.static-library'},
    {id = 'platform-handlers', title = 'Platform handlers', description = 'Kotlin, Java, Swift and JavaScript handlers that answer, refuse, throw and stop when the app cancels.', module = 'tests.platform-handlers'},
}
