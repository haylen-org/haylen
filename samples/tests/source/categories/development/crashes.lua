-- Crashes and breadcrumbs: breadcrumbs that the event bus publishes for plugins, the report of the native crash that ended the last run, and a button that crashes the app natively so the next start shows its report. The panel holds the report of the last crash, and the card beside it the kept breadcrumbs with their time and data.
local debugging = require('haylen.debug')
local haylen = require('haylen')
local json = require('json')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Test = require('harness.test')
local LogCard = require('categories.development.log-card')

local Crashes = haylen.class('Crashes', Test)

Crashes.code = [[
debugging.addBreadcrumb('Opened the shop.', {items = 12})
events.on('breadcrumbAdded', function(crumb) reporter.add(crumb.message, crumb.data) end, {owner = self})  -- A crash reporting plugin forwards it.
local crash = debugging.lastCrash()  -- The native crash that ended the last run, or nil.
if crash then print(crash.reason, #crash.context.breadcrumbs) end]]

function Crashes:enter()
    self.heard = 0
    self.crumbs = LogCard(self, 'crumbs', 'Breadcrumbs', 'The breadcrumbs of this run show here.')
    self:listen('breadcrumbAdded', function()
        self.heard = self.heard + 1
        self:set('heard', {text = self.heard == 1 and 'The event "breadcrumbAdded" arrived once.' or string.format('The event "breadcrumbAdded" arrived %d times.', self.heard)})
    end)
    local crash = debugging.lastCrash()
    local report
    if crash then
        local crumbs = crash.context.breadcrumbs or {}
        local last = crumbs[#crumbs]
        report = ui.alert{tone = 'danger', title = string.format('The last run stopped with "%s"', crash.reason), message = string.format('It left %d breadcrumbs, the last one "%s".', #crumbs, last and last.message or 'none')}
    else
        report = ui.alert{tone = 'success', title = 'No crash', message = 'The last run ended without a native crash.'}
    end
    self:frame{
        hint = 'Add breadcrumbs, then crash the app and start it again to read the report of the crash.',
        code = Crashes.code,
        content = {ui.row{wrap = true, grow = 1, gap = 24, alignItems = 'stretch',
            ui.panel{width = 560, gap = 12,
                ui.label{text = 'The last run', font = 'heading'},
                report,
                ui.label{id = 'heard', text = 'No breadcrumb arrived yet.', color = 'textMuted'},
                ui.button{id = 'add', text = 'Add a breadcrumb', align = 'stretch', onClick = function() self:add() end},
                ui.button{id = 'crash', text = 'Crash the app natively', align = 'stretch', variant = 'destructive', onClick = function() self:crash() end},
            },
            self.crumbs:node(),
        }},
    }
    self:add()
end

function Crashes:add()
    local count = #debugging.breadcrumbs() + 1
    debugging.addBreadcrumb('Pressed the test button ' .. count .. '.', {test = 'DEV-010', press = count})
    self:showCrumbs()
end

-- Lists the kept breadcrumbs, the newest first, each with its time and its data.
function Crashes:showCrumbs()
    self.crumbs.entries = {}
    for _, crumb in ipairs(debugging.breadcrumbs()) do
        self.crumbs:add(string.format('%s  %s', os.date('%H:%M:%S', math.floor(crumb.time)), crumb.message), crumb.data and 'Data ' .. json.encode(crumb.data) or nil)
    end
end

-- Reading address zero through the foreign function interface ends the process with a fault that only the native guard sees.
function Crashes:crash()
    local found, ffi = pcall(require, 'ffi')
    if not found then
        self.crumbs:add('No crash', 'This platform has no "ffi" module, so the test cannot crash it natively.', 'danger')
        return
    end
    debugging.addBreadcrumb('Crashed the app on purpose.')
    timer.after(0.1, function()
        print(ffi.cast('int*', 0)[0])
    end, {owner = self})
end

return Crashes
