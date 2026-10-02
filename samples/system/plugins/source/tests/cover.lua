-- Covering native UI: the plugin shows a native screen over the whole app and covers the app while it shows. The covered app is inactive, halted and muted, so haylen.appCovered() is true, its updates stop and the engine draws its covered state once and keeps it on screen, appInactive arrives when the screen shows and appActive once Close ends the cover.
local haylen = require('haylen')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Cover = haylen.class('Cover', sample.Test)

local kRows = {
    {key = 'covered', name = 'The call "haylen.appCovered()" returns "true" while it shows', waiting = 'Waiting for the cover'},
    {key = 'halted', name = 'No update runs while covered', waiting = 'Waiting for the cover to end'},
    {key = 'events', name = 'The events "appInactive" and "appActive" arrive', waiting = 'Waiting for the app state to change'},
}

function Cover:enter()
    self.updates = 0
    self:listen('appInactive', function() self:changed('appInactive') end)
    self:listen('appActive', function() self:changed('appActive') end)
    self:frame({
        hint = 'Show the native screen, wait a moment and close it.',
        focus = 'show',
        controls = {
            ui.button{id = 'show', text = 'Show the native screen', variant = 'primary', onClick = function() self:show() end},
            ui.label{text = 'A presented view controller on iOS, iPadOS, Mac Catalyst and tvOS, a sheet on macOS, a full screen dialog on Android and a modal dialog element on the web. The remote of a TV, gamepads and the keyboard reach its Close button, since it is native UI that takes the focus.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "events.on('appInactive', function()\n  print(haylen.appCovered())\nend)\nlocal closed = demo.showScreen('Title'):await()\nprint(closed.seconds)"},
        },
    })
end

function Cover:show()
    self:act(function()
        local name = 'The native screen closes'
        self.states = {}
        self.cover = {}
        self.results:set('screen', 'waiting', name, 'The native screen shows. Close it with its Close button.')
        for _, row in ipairs(kRows) do
            self.results:set(row.key, 'waiting', row.name, row.waiting)
        end
        local closed, err = demo.showScreen('Haylen Plugins'):await()
        if err then
            self.results:failure('screen', name, err)
            for _, row in ipairs(kRows) do
                self.results:set(row.key, 'skip', row.name, 'The platform shows no native screen.')
            end
            return
        end
        self.results:set('screen', 'pass', name, string.format('The screen showed for %.1f seconds and answered once Close closed it.', closed.seconds))
    end)
end

-- The app hears appInactive when the screen takes the focus or when the cover starts, whichever comes first, and appActive once both ended.
function Cover:changed(name)
    if not self.states then
        return
    end
    self.states[#self.states + 1] = name
    if #self.states >= 2 and self.states[1] == 'appInactive' and self.states[#self.states] == 'appActive' then
        self.results:set('events', 'pass', kRows[3].name, 'The events "' .. table.concat(self.states, '", "') .. '" arrived around the cover.')
    end
end

function Cover:update(dt)
    Cover.super.update(self, dt)
    self.updates = self.updates + 1
    if self.cover and self.cover.frames and not haylen.appCovered() and not self.cover.ended then
        self.cover.ended = true
        local during = self.updates - 1 - self.cover.updates
        if during == 0 then
            self.results:set('halted', 'pass', kRows[2].name, string.format('Update %d ran before the cover, none ran during its %d frames, and update %d ran after it ended.', self.cover.updates, self.cover.frames, self.updates))
        else
            self.results:set('halted', 'fail', kRows[2].name, string.format('%d updates ran during the %d frames of the cover.', during, self.cover.frames))
        end
    end
end

-- A covered app is halted, so its updates stop, and the engine draws it once more under the native screen, where the render hook sees the cover, and keeps that frame on screen.
function Cover:render()
    Cover.super.render(self)
    if not self.cover or not haylen.appCovered() then
        return
    end
    if not self.cover.frames then
        self.cover.frames = 0
        self.cover.updates = self.updates
        self.results:set('covered', 'pass', kRows[1].name, string.format('The cover started after update %d with the app state "%s".', self.updates, haylen.appState()))
    end
    self.cover.frames = self.cover.frames + 1
end

return Cover
