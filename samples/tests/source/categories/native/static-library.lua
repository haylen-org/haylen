-- The iOS and tvOS apps link `native_test_static` into the app, and `haylen.py` writes a table of its symbols that `native.load` and `native.findSymbol` find, while Varn `ffi` calls it through `ffi.C`.
local ffi = require('ffi')
local haylen = require('haylen')
local native = require('haylen.native')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Checks = require('categories.native.checks')
-- The C declarations of the test library, which calls through `ffi.C` need too.
require('categories.native.library')

local StaticLibrary = haylen.class('StaticLibrary', Test)

local expect = Checks.expect

function StaticLibrary:enter()
    self:frame{
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'The "native" section of "app.json" links "native_test_static" into iOS and tvOS apps and lists the symbols Lua reaches, which the generated "HaylenNativeSymbols.mm" keeps and registers. The function "native.load" returns "ffi.C" for a linked library, because its symbols are part of the app.', color = 'textMuted', font = 'caption'},
        },
        code = "local lib = native.load('native_test_static')\nprint(ffi.string(lib.native_test_origin()))",
    }
    self:run()
end

function StaticLibrary:run()
    self.checks = Checks(self.entry.code)
    self:spawn(function()
        local checks = self.checks
        local lib
        checks:run('The linked library', function()
            lib = native.load('native_test_static')
            expect(lib, ffi.C, 'the namespace of the library')
            return 'The function "native.load" returned "ffi.C".'
        end)
        checks:run('The symbol table', function()
            local address = native.findSymbol('native_test_origin')
            if address == nil then
                error('The function "native.findSymbol" found no "native_test_origin".', 0)
            end
            return 'The function "native.findSymbol" found "native_test_origin" at ' .. tostring(address) .. '.'
        end)
        checks:run('Text from the linked copy', function()
            return 'The call "native_test_origin()" returned "' .. expect(ffi.string(lib.native_test_origin()), 'static', 'the origin') .. '".'
        end)
        checks:run('Integers', function()
            return 'The call "native_test_add(2, 3)" returned ' .. expect(lib.native_test_add(2, 3), 5, 'the sum') .. '.'
        end)
    end)
end

function StaticLibrary:update(dt)
    StaticLibrary.super.update(self, dt)
    self:status(self.checks:summary())
end

function StaticLibrary:draw(area)
    self.checks:draw(area)
end

return StaticLibrary
