-- Static library: iOS and tvOS apps link native_test_static into the app, and make.py writes a table of its symbols that native.load and native.findSymbol find, while Varn ffi calls it through ffi.C. The other platforms load dynamic libraries only, and the browser none.
local ffi = require('ffi')
local haylen = require('haylen')
local native = require('haylen.native')
local ui = require('haylen.ui')

local CheckList = require('check-list')
local sample = require('sample')

local StaticLibrary = haylen.class('StaticLibrary', sample.Test)

local expect = CheckList.expect
local kLinked = {ios = true, tvos = true}

function StaticLibrary:enter()
    self:frame({
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'The native section of app.json links native_test_static into iOS and tvOS apps and lists the symbols Lua reaches, which the generated HaylenNativeSymbols.mm keeps and registers. native.load returns ffi.C for a linked library, because its symbols are part of the app.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "local lib = native.load('native_test_static')\nprint(ffi.string(lib.native_test_origin()))"},
        },
    })
    self:run()
end

function StaticLibrary:run()
    self.checks = CheckList(self.info.id)
    self:spawn(function()
        local checks = self.checks
        if not native.available() then
            checks:skip('Linked library', 'The browser links no native libraries.')
            return
        end
        if not kLinked[haylen.platform] then
            checks:skip('Linked library', 'Static libraries link into iOS and tvOS apps, and ' .. haylen.platform .. ' loads dynamic libraries.')
            return
        end

        local lib
        checks:run('Linked library', function()
            lib = native.load('native_test_static')
            expect(lib, ffi.C, 'the namespace of the library')
            return 'native.load returned ffi.C'
        end)
        checks:run('Symbol table', function()
            local address = native.findSymbol('native_test_origin')
            if address == nil then
                error('native.findSymbol found no native_test_origin', 0)
            end
            return 'native.findSymbol found native_test_origin at ' .. tostring(address)
        end)
        checks:run('Text from the linked copy', function()
            return 'native_test_origin() = ' .. expect(ffi.string(lib.native_test_origin()), 'static', 'the origin')
        end)
        checks:run('Integers', function()
            return 'native_test_add(2, 3) = ' .. expect(lib.native_test_add(2, 3), 5, 'the sum')
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
