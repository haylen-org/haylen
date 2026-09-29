-- Calls: numbers, text, structs by value and by pointer, a buffer and a symbol of the test library through Varn ffi. The browser has no native libraries, so there the page answers the same functions through the bridge.
local ffi = require('ffi')
local haylen = require('haylen')
local native = require('haylen.native')
local ui = require('haylen.ui')

local CheckList = require('check-list')
local library = require('test-library')
local sample = require('sample')

local Calls = haylen.class('Calls', sample.Test)

local expect = CheckList.expect

function Calls:enter()
    self:frame({
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'native.load finds native_test next to the app, loads it and hands it to Varn ffi, which calls the functions that ffi.cdef declares with the C types of their parameters and results.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "local lib = native.load('native_test')\nprint(lib.native_test_add(20, 22))"},
        },
    })
    self:run()
end

function Calls:run()
    self.checks = CheckList(self.info.id)
    self:spawn(function()
        if native.available() then
            self:runLibrary()
        else
            self:runPage()
        end
    end)
end

function Calls:runLibrary()
    local checks = self.checks
    local lib
    checks:run('Load by name', function()
        lib = library.load()
        return 'native.load returned ' .. tostring(lib)
    end)
    checks:run('Integers', function() return 'native_test_add(20, 22) = ' .. expect(lib.native_test_add(20, 22), 42, 'the sum') end)
    checks:run('Doubles', function() return 'native_test_scale(1.5, 4) = ' .. expect(lib.native_test_scale(1.5, 4), 6.0, 'the product') end)
    checks:run('Text', function() return 'native_test_origin() = ' .. expect(ffi.string(lib.native_test_origin()), 'dynamic', 'the origin') end)
    checks:run('Struct by value', function()
        local sum = lib.native_test_point_add(ffi.new('NativeTestPoint', {1, 2}), ffi.new('NativeTestPoint', {x = 10, y = 20}))
        return string.format('(1, 2) + (10, 20) = (%d, %d)', expect(sum.x, 11, 'x'), expect(sum.y, 22, 'y'))
    end)
    checks:run('Struct by pointer', function()
        local rect = ffi.new('NativeTestRect', {origin = {5, 5}, width = 10, height = 4})
        lib.native_test_rect_grow(rect, 2)
        return string.format('grown to (%d, %d) with a size of %g by %g', expect(rect.origin.x, 3, 'x'), expect(rect.origin.y, 3, 'y'), expect(rect.width, 14, 'the width'), expect(rect.height, 8, 'the height'))
    end)
    checks:run('Buffer', function()
        local buffer = ffi.new('uint8_t[?]', 8)
        lib.native_test_fill(buffer, 8, 250)
        local bytes = ffi.string(buffer, 8)
        local checksum = lib.native_test_checksum(ffi.cast('const uint8_t*', buffer), 8)
        return string.format('filled %s with checksum %08x', bytes:gsub('.', function(byte) return string.format('%02x ', byte:byte()) end), expect(checksum, library.checksum(bytes), 'the checksum'))
    end)
    checks:run('Symbol', function()
        local address = native.symbol('native_test_add')
        if address == nil then
            error('native.symbol found no native_test_add', 0)
        end
        return 'native.symbol found native_test_add at ' .. tostring(address)
    end)
end

function Calls:runPage()
    local checks = self.checks
    checks:skip('Load by name', 'The browser loads no native libraries, so platform/web/app.js answers the same functions through the bridge.')
    checks:run('Integers', function() return 'native_test.add = ' .. expect(sample.call('native_test.add', {a = 20, b = 22}), 42, 'the sum') end)
    checks:run('Doubles', function() return 'native_test.scale = ' .. expect(sample.call('native_test.scale', {value = 1.5, factor = 4}), 6, 'the product') end)
    checks:run('Text', function() return 'native_test.origin = ' .. expect(sample.call('native_test.origin'), 'javascript', 'the origin') end)
    checks:run('Struct by value', function()
        local sum = sample.call('native_test.point_add', {a = {x = 1, y = 2}, b = {x = 10, y = 20}})
        return string.format('(1, 2) + (10, 20) = (%d, %d)', expect(sum.x, 11, 'x'), expect(sum.y, 22, 'y'))
    end)
    checks:run('Struct by pointer', function()
        local rect = sample.call('native_test.rect_grow', {rect = {origin = {x = 5, y = 5}, width = 10, height = 4}, amount = 2})
        return string.format('grown to (%d, %d) with a size of %g by %g', expect(rect.origin.x, 3, 'x'), expect(rect.origin.y, 3, 'y'), expect(rect.width, 14, 'the width'), expect(rect.height, 8, 'the height'))
    end)
    checks:run('Buffer', function()
        local filled = sample.call('native_test.fill', {size = 8, seed = 250})
        local bytes = string.char(table.unpack(filled))
        return string.format('filled %d bytes with checksum %08x', #bytes, expect(sample.call('native_test.checksum', {bytes = filled}), library.checksum(bytes), 'the checksum'))
    end)
    checks:skip('Symbol', 'The browser has no symbols to look up.')
end

function Calls:update(dt)
    Calls.super.update(self, dt)
    self:status(self.checks:summary())
end

function Calls:draw(area)
    self.checks:draw(area)
end

return Calls
