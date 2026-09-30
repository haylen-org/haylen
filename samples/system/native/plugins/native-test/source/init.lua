-- Lua API of the stand-in of the test library of the engine in the browser, which loads no native libraries, so the web part of this plugin answers the functions and the handlers of the library as its own methods. The sample loads it with require('native-test').
local native = require('haylen.native')
local platform = require('haylen.platform')

local handle = platform.plugin('native-test')
local nativeTest = {}

-- The name of the handler or event `name` of the library on this platform: the library registers native_test.<name> where it loads, and this plugin answers native-test.<name> in the browser.
function nativeTest.name(name)
    return (native.available() and 'native_test.' or 'native-test.') .. name
end

-- Calls the function `name` of the library in the browser, such as add, pointAdd or reportLater, which the page answers with its result.
function nativeTest.call(name, params)
    return handle:call(name, params)
end

return nativeTest
