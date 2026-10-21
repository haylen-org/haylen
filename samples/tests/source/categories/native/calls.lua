-- Numbers, text, structs by value and by pointer, a buffer and a symbol of the test library through Varn `ffi`. The browser has no native libraries, so there the page answers the same functions through the bridge.
local ffi = require('ffi')
local haylen = require('haylen')
local native = require('haylen.native')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Checks = require('categories.native.checks')
local library = require('categories.native.library')
local nativeTest = require('native-test')

local Calls = haylen.class('Calls', Test)

local expect = Checks.expect

function Calls:enter()
    self.checks = Checks(self.entry.code)
    self:frame{
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        content = {self.checks:view()},
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'The function "native.load" finds "native-test" next to the app, loads it and hands it to Varn "ffi", which calls the functions that "ffi.cdef" declares with the C types of their parameters and results.', color = 'textMuted', font = 'caption'},
        },
        code = "local lib = native.load('native-test')\nprint(lib.native_test_add(20, 22))\nlocal sum = lib.native_test_point_add(ffi.new('NativeTestPoint', {1, 2}), other)\nlib.native_test_rect_grow(rect, 2)  -- Changes the struct through its pointer.\nlocal buffer = ffi.new('uint8_t[?]', 8)\nlib.native_test_fill(buffer, 8, 250)",
    }
    self.checks:show(self.gui)
    self:run()
end

function Calls:run()
    self.checks:clear()
    self:spawn(function()
        if native.available() then
            self:runLibrary()
        else
            self:runPage()
        end
    end)
end

-- The checks that call into the library, which have nothing to call when it did not load.
Calls.libraryChecks = {'Integers', 'Doubles', 'Text', 'Struct by value', 'Struct by pointer', 'Buffer', 'Symbol'}

function Calls:runLibrary()
    local checks = self.checks
    local lib
    checks:run('Load by name', function()
        lib = library.load()
        return 'The function "native.load" returned ' .. tostring(lib) .. '.'
    end)
    if not lib then
        checks:failEach(Calls.libraryChecks, 'The library did not load, so this call did not run.')
        return
    end
    checks:run('Integers', function() return 'The call "native_test_add(20, 22)" returned ' .. expect(lib.native_test_add(20, 22), 42, 'the sum') .. '.' end)
    checks:run('Doubles', function() return 'The call "native_test_scale(1.5, 4)" returned ' .. expect(lib.native_test_scale(1.5, 4), 6.0, 'the product') .. '.' end)
    checks:run('Text', function() return 'The call "native_test_origin()" returned "' .. expect(ffi.string(lib.native_test_origin()), 'dynamic', 'the origin') .. '".' end)
    checks:run('Struct by value', function()
        local sum = lib.native_test_point_add(ffi.new('NativeTestPoint', {1, 2}), ffi.new('NativeTestPoint', {x = 10, y = 20}))
        return string.format('The sum of (1, 2) and (10, 20) is (%d, %d).', expect(sum.x, 11, '"x"'), expect(sum.y, 22, '"y"'))
    end)
    checks:run('Struct by pointer', function()
        local rect = ffi.new('NativeTestRect', {origin = {5, 5}, width = 10, height = 4})
        lib.native_test_rect_grow(rect, 2)
        return string.format('Grown to (%d, %d) with a size of %g by %g.', expect(rect.origin.x, 3, '"x"'), expect(rect.origin.y, 3, '"y"'), expect(rect.width, 14, 'the width'), expect(rect.height, 8, 'the height'))
    end)
    checks:run('Buffer', function()
        local buffer = ffi.new('uint8_t[?]', 8)
        lib.native_test_fill(buffer, 8, 250)
        local bytes = ffi.string(buffer, 8)
        local checksum = lib.native_test_checksum(buffer, 8)
        local hex = bytes:gsub('.', function(byte) return string.format('%02x ', byte:byte()) end)
        return string.format('Filled %swith the checksum %08x.', hex, expect(checksum, library.checksum(bytes), 'the checksum'))
    end)
    checks:run('Symbol', function()
        local address = native.findSymbol('native_test_add')
        if address == nil then
            error('The function "native.findSymbol" found no "native_test_add".', 0)
        end
        local add = ffi.cast('int32_t (*)(int32_t, int32_t)', address)
        return 'The function "native.findSymbol" found "native_test_add" at ' .. tostring(address) .. ', and a call through it returned ' .. expect(add(2, 3), 5, 'the sum') .. '.'
    end)
end

function Calls:runPage()
    local checks = self.checks
    checks:run('Integers', function() return 'The method "native-test.add" returned ' .. expect(Checks.await(nativeTest.call('add', {a = 20, b = 22})), 42, 'the sum') .. '.' end)
    checks:run('Doubles', function() return 'The method "native-test.scale" returned ' .. expect(Checks.await(nativeTest.call('scale', {value = 1.5, factor = 4})), 6, 'the product') .. '.' end)
    checks:run('Text', function() return 'The method "native-test.origin" returned "' .. expect(Checks.await(nativeTest.call('origin')), 'javascript', 'the origin') .. '".' end)
    checks:run('Struct by value', function()
        local sum = Checks.await(nativeTest.call('pointAdd', {a = {x = 1, y = 2}, b = {x = 10, y = 20}}))
        return string.format('The sum of (1, 2) and (10, 20) is (%d, %d).', expect(sum.x, 11, '"x"'), expect(sum.y, 22, '"y"'))
    end)
    checks:run('Struct by pointer', function()
        local rect = Checks.await(nativeTest.call('rectGrow', {rect = {origin = {x = 5, y = 5}, width = 10, height = 4}, amount = 2}))
        return string.format('Grown to (%d, %d) with a size of %g by %g.', expect(rect.origin.x, 3, '"x"'), expect(rect.origin.y, 3, '"y"'), expect(rect.width, 14, 'the width'), expect(rect.height, 8, 'the height'))
    end)
    checks:run('Buffer', function()
        local filled = Checks.await(nativeTest.call('fill', {size = 8, seed = 250}))
        local bytes = string.char(table.unpack(filled))
        return string.format('Filled %d bytes with the checksum %08x.', #bytes, expect(Checks.await(nativeTest.call('checksum', {bytes = filled})), library.checksum(bytes), 'the checksum'))
    end)
    checks:skip('Loading and symbols', 'The browser loads no native libraries and has no symbols to look up, so the web part of the plugin "native-test" answers the same functions through the bridge.')
end

function Calls:update(dt)
    Calls.super.update(self, dt)
    self:status(self.checks:summary())
end

return Calls
