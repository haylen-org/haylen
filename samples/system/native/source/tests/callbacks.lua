-- Callbacks: Lua functions that the test library calls during a call, at the next frame and from a thread of its own, through Varn `ffi` and `haylen.native`. The browser has no native callbacks, so there the page sends the same report as an event.
local ffi = require('ffi')
local haylen = require('haylen')
local native = require('haylen.native')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local CheckList = require('check-list')
local library = require('test-library')
local sample = require('sample')

local Callbacks = haylen.class('Callbacks', sample.Test)

local expect = CheckList.expect
local kReport = 'void (int32_t value, const uint8_t data[size], size_t size)'

function Callbacks:enter()
    self:frame({
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = "A \"native.callback\" copies its arguments and runs the Lua function on the frame thread: during the call with \"thread = 'frame'\", or at the next frame, which is always the case for a call from another thread. Varn \"ffi.cast\" callbacks run during the call on the thread that makes it.", color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "local report = native.callback(\n  'void (int32_t value, const uint8_t data[size], size_t size)',\n  function(value, data, size) print(value, #data) end)\nlib.native_test_report_later(report.pointer, 42)"},
        },
    })
    self:run()
end

-- Callbacks live until they are freed, so the ones of the previous run go before new ones take their place.
function Callbacks:run()
    self:free()
    self.checks = CheckList(self.info.id)
    self:spawn(function()
        if native.available() then
            self:runLibrary()
        else
            self:runPage()
        end
    end)
end

function Callbacks:runLibrary()
    local checks = self.checks
    local lib = library.load()
    local visits = {}
    local function visitor(prefix)
        return function(index, label)
            visits[#visits + 1] = prefix .. index .. (type(label) == 'string' and label or ffi.string(label))
        end
    end

    checks:run('ffi.cast during the call', function()
        self.cast = ffi.cast('NativeTestVisitor', visitor('ffi '))
        lib.native_test_visit(2, self.cast)
        return 'Visited ' .. expect(table.concat(visits, ', '), 'ffi 0zero, ffi 1one', 'the visits')
    end)
    checks:run('Frame callback during the call', function()
        visits = {}
        self.atOnce = native.callback('void (int32_t index, const char* label)', visitor('frame '), {thread = 'frame'})
        lib.native_test_visit(2, self.atOnce.pointer)
        return 'Visited ' .. expect(table.concat(visits, ', '), 'frame 0zero, frame 1one', 'the visits')
    end)
    checks:run('Callback at the next frame', function()
        visits = {}
        self.later = native.callback('void (int32_t index, const char* label)', visitor('next '))
        local frame = haylen.frameIndex()
        lib.native_test_visit(2, self.later.pointer)
        expect(#visits, 0, 'the visits during the call')
        sample.waitFor(function() return #visits == 2 end, 1)
        expect(table.concat(visits, ', '), 'next 0zero, next 1one', 'the visits of the next frame')
        return string.format('Visited %s at frame %d after the call at frame %d', table.concat(visits, ', '), haylen.frameIndex(), frame)
    end)

    local report
    checks:run('Callback from a library thread', function()
        self.reporter = native.callback(kReport, function(value, data, size) report = {value = value, data = data, size = size} end)
        lib.native_test_report_later(self.reporter.pointer, 42)
        if not sample.waitFor(function() return report ~= nil end, 2) then
            error('No report arrived within two seconds.', 0)
        end
        return 'The thread of the library reported ' .. expect(report.value, 42, 'the value') .. ' on the frame thread'
    end)
    checks:run('Bytes bounded by a length', function()
        if report == nil then
            error('No report arrived.', 0)
        end
        expect(report.size, 4, 'the size')
        return 'Received ' .. expect(report.data, '\1\2\3\4', 'the bytes'):gsub('.', function(byte) return string.format('%02x ', byte:byte()) end)
    end)
end

function Callbacks:runPage()
    local checks = self.checks
    checks:skip('ffi.cast during the call', 'The browser has no native callbacks.')
    checks:skip('Frame callback during the call', 'The browser has no native callbacks.')
    checks:skip('Callback at the next frame', 'The browser has no native callbacks.')
    local report
    checks:run('Report from the page', function()
        local connection = platform.on('native_test.report', function(payload) report = payload end)
        sample.call('native_test.report_later', {value = 42})
        local arrived = sample.waitFor(function() return report ~= nil end, 2)
        connection:disconnect()
        if not arrived then
            error('No report arrived within two seconds.', 0)
        end
        return 'The page reported ' .. expect(report.value, 42, 'the value') .. ' through an event'
    end)
    checks:run('Bytes of the report', function()
        return 'Received ' .. expect(table.concat(report.data, ' '), '1 2 3 4', 'the bytes')
    end)
end

function Callbacks:free()
    for _, name in ipairs({'atOnce', 'later', 'reporter'}) do
        if self[name] then
            self[name]:free()
            self[name] = nil
        end
    end
end

function Callbacks:exit()
    self:free()
end

function Callbacks:update(dt)
    Callbacks.super.update(self, dt)
    self:status(self.checks:summary())
end

function Callbacks:draw(area)
    self.checks:draw(area)
end

return Callbacks
