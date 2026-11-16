-- The Lua API of the plugin, which calls the part of the plugin that the backend of the test port links into the app.
local ffi = require('ffi')
local native = require('haylen.native')

ffi.cdef([[
    const char* test_port_greeter_greeting(void);
]])

local library = native.load('test-port-greeter')
local greeter = {}

function greeter.greeting()
    return ffi.string(library.test_port_greeter_greeting())
end

return greeter
