-- Crashes and breadcrumbs: breadcrumbs that the event bus publishes for plugins, the report of the native crash that ended the last run, and a button that crashes the app natively so the next start shows its report.
local debugging = require('haylen.debug')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Crashes = haylen.class('Crashes', Test)

function Crashes:enter()
    self.lines = {}
    self.heard = 0
    self:listen('breadcrumbAdded', function()
        self.heard = self.heard + 1
    end)
    self:frame{
        hint = 'Add breadcrumbs, then crash the app and start it again to read the report of the crash.',
        content = {ui.row{wrap = true, grow = 1, gap = 24,
            ui.panel{width = 520, align = 'stretch', gap = 12,
                ui.button{id = 'add', text = 'Add a breadcrumb', align = 'stretch', onClick = function() self:add() end},
                ui.button{id = 'crash', text = 'Crash the app natively', align = 'stretch', variant = 'destructive', onClick = function() self:crash() end},
                ui.label{font = 'monospace', text = "debug.addBreadcrumb(message, data)\ndebug.breadcrumbs()\ndebug.lastCrash()"},
            },
            ui.scroll{grow = 1, ui.label{id = 'log', text = '', grow = 1}},
        }},
    }
    local crash = debugging.lastCrash()
    if crash then
        local crumbs = crash.context.breadcrumbs or {}
        local last = crumbs[#crumbs]
        self:note(string.format('The last run stopped with "%s" after %d breadcrumbs, the last one "%s".', crash.reason, #crumbs, last and last.message or 'none'))
    else
        self:note('The last run ended without a native crash.')
    end
    self:add()
end

function Crashes:note(line)
    table.insert(self.lines, 1, line)
    self.lines[13] = nil
    self:set('log', {text = table.concat(self.lines, '\n')})
end

function Crashes:add()
    local count = #debugging.breadcrumbs() + 1
    debugging.addBreadcrumb('Pressed the test button ' .. count .. '.', {test = 'DEV-010'})
    self:note(string.format('Added breadcrumb %d. The event "breadcrumbAdded" arrived %d times before this one.', count, self.heard))
end

-- Reading address zero through the foreign function interface ends the process with a fault that only the native guard sees.
function Crashes:crash()
    local found, ffi = pcall(require, 'ffi')
    if not found then
        self:note('This platform has no "ffi" module, so the test cannot crash it natively.')
        return
    end
    debugging.addBreadcrumb('Crashed the app on purpose.')
    require('haylen.timer').after(0.1, function()
        print(ffi.cast('int*', 0)[0])
    end)
end

return Crashes
