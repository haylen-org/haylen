-- Native screen: the plugin opens a screen of its own, a UIKit controller or an AppKit sheet on Apple platforms, an AndroidX activity on Android, a popup page on the web and a native window over the window of the app on the desktops, and its answer reaches the call. Apple platforms add the same question in SwiftUI. The engine covers the app before the screen shows, so the app is inactive, halted and muted, and it draws nothing under the opaque screen. A second screen fails with busy while one shows, and a timer of the app gives a screen up while it shows, which closes it. A screen whose app restarts before it ends, the redirect screen of the web, whose page loads again, and the screen of an Android app whose process ended under it reach the next app as screenRestored with the state the app gave. A screen opened right in the answer of a message waits until the app is active and then opens.
local async = require('async')
local dialogs = require('haylen.dialogs')
local haylen = require('haylen')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Screen = haylen.class('Screen', sample.Test)

-- The restored end outlives the scene, because a retained event reaches only the listener that connects first.
local restored = nil

local kRows = {
    {key = 'result', name = 'The screen answers the call', waiting = 'Open the screen and confirm or decline it.'},
    {key = 'covered', name = 'The app is covered while the screen shows', waiting = 'Open the screen to see the cover.'},
    {key = 'busy', name = 'A second screen fails with "busy"', waiting = 'Open the screen to try a second one.'},
    {key = 'cancel', name = 'A cancel closes the screen', waiting = 'Open and cancel after a second.'},
    {key = 'restored', name = 'The event "screenRestored" brings the end and the state to the next app', waiting = 'Open, then restart the app, answer the screen and open this test again.'},
    {key = 'redirect', name = 'The redirect screen comes back as "screenRestored"', waiting = 'Open by redirect, answer the page and open this test again.'},
    {key = 'swiftUI', name = 'The SwiftUI screen answers the call', waiting = 'Open the SwiftUI screen and answer or close it.'},
    {key = 'dialog', name = 'A screen opens right in the answer of a message', waiting = 'Open from the answer of a message, then answer the message and the screen.'},
}

function Screen:enter()
    self:listen('appInactive', function() self:covered() end)
    self:listen('appActive', function() self:uncovered() end)
    self:frame({
        hint = 'Open the screen and answer it, then open and restart the app.',
        focus = 'open',
        controls = {
            ui.button{id = 'open', text = 'Open the screen', variant = 'primary', onClick = function() self:open() end},
            ui.button{id = 'cancel', text = 'Open and cancel after a second', onClick = function() self:cancelLater() end},
            ui.button{id = 'restart', text = 'Open, then restart the app', onClick = function() self:openAndRestart() end},
            ui.button{id = 'redirect', text = 'Open by redirect', onClick = function() self:openRedirect() end},
            ui.button{id = 'swiftUI', text = 'Open the SwiftUI screen', onClick = function() self:openSwiftUI() end},
            ui.button{id = 'dialog', text = 'Open from the answer of a message', onClick = function() self:openFromDialog() end},
            ui.label{text = 'A UIKit controller on iOS, iPadOS, Mac Catalyst and tvOS, an AndroidX activity on Android, a popup page on the web, which a popup blocker stops when the tap is too long ago, and a sheet on macOS, an owned window on Windows and a transient X11 window on Linux. The redirect leaves the page for a page of the plugin, which only the web has, and the SwiftUI screen shows over the app on iOS, iPadOS and tvOS and in a window of its own on Mac Catalyst and macOS.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "local answer, err = demo.openScreen({state = {level = 3}}):await()\ndemo.onScreenRestored(function(ending)\n  print(ending.state.level, ending.result)\nend)"},
        },
    })
    if not self.native then
        return
    end
    for _, row in ipairs(kRows) do
        self.results:set(row.key, 'waiting', row.name, row.waiting)
    end
    self.connection = demo.onScreenRestored(function(ending)
        restored = ending
        self:showRestored()
    end)
    self:showRestored()
end

function Screen:exit()
    if self.connection then
        self.connection:disconnect()
    end
end

function Screen:open()
    self:act(function()
        local name = kRows[1].name
        self.results:set('result', 'waiting', name, 'The screen shows. Confirm or decline it.')
        self.watch = {renders = 0, updates = 0}
        local call = demo.openScreen({state = {test = 'screen', openedAt = os.date('%H:%M:%S')}})

        -- The first screen shows from the moment the app asked for it, so a second one fails at once.
        local _, busy = demo.openScreen():await()
        if busy and busy.code == 'busy' then
            self.results:set('busy', 'pass', kRows[3].name, busy.message)
        else
            self.results:set('busy', 'fail', kRows[3].name, 'The second screen did not fail with "busy": ' .. tostring(busy and busy.code))
        end

        local answer, err = call:await()
        if err and err.code == 'cancelled' then
            self.results:set('result', 'info', name, 'The person closed the screen without an answer, which the call hears as "cancelled": ' .. err.message)
            return
        elseif err then
            self.results:failure('result', name, err)
            if err.code == 'unsupported' then
                self.watch = nil
                self.results:set('covered', 'skip', kRows[2].name, 'No screen shows on this platform yet.')
            end
            return
        end
        self.results:set('result', 'pass', name, string.format('The person %s through %s in %s.', answer.confirmed and 'confirmed' or 'declined', answer.via, answer.language))
    end)
end

function Screen:cancelLater()
    self:act(function()
        local name = kRows[4].name
        self.results:set('cancel', 'waiting', name, 'The screen shows and closes after a second.')
        local call = demo.openScreen()
        async.sleep(1000):await()
        local pending = call:cancel()
        local _, err = call:await()
        if not pending then
            if err then
                self.results:failure('cancel', name, err)
            else
                self.results:set('cancel', 'fail', name, 'The screen ended before the cancel.')
            end
            return
        end
        sample.waitFor(function() return not haylen.appCovered() end, 5)
        self.results:set('cancel', err.code == 'cancelled' and not haylen.appCovered() and 'pass' or 'fail', name, string.format('The call failed with "%s", and the cover ended once the screen was gone.', tostring(err.code)))
    end)
end

-- The app restarts a second after the screen showed, from a timer of the halted app, and the screen keeps showing and covers the next app, whose screenRestored listener receives the answer.
function Screen:openAndRestart()
    self:act(function()
        demo.openScreen({state = {test = 'screen', restartedAt = os.date('%H:%M:%S')}})
        self.results:set('restored', 'waiting', kRows[5].name, 'The app restarts under the screen. Answer it and open this test again.')
        sample.waitFor(function() return platform.screenShowing() and haylen.appCovered() end, 2)
        async.sleep(1000):await()
        haylen.requestRestart()
    end)
end

-- The page leaves for the confirm page of the plugin, so nothing after the redirect runs in this app.
function Screen:openRedirect()
    self:act(function()
        if haylen.platform ~= 'web' then
            self.results:set('redirect', 'skip', kRows[6].name, 'Only the web opens a screen by redirect.')
            return
        end
        local _, err = demo.openRedirectScreen({state = {test = 'screen', redirectedAt = os.date('%H:%M:%S')}}):await()
        if err then
            self.results:failure('redirect', kRows[6].name, err)
        end
    end)
end

-- The SwiftUI screen lets the app show through, as a sheet over it on iOS, iPadOS and tvOS, where a swipe closes it too. It answers like the confirm screen, and its Close button dismisses it through SwiftUI, which the call hears as `cancelled`.
function Screen:openSwiftUI()
    self:act(function()
        local name = kRows[7].name
        self.results:set('swiftUI', 'waiting', name, 'The SwiftUI screen shows. Answer or close it.')
        local answer, err = demo.openSwiftUIScreen({opaque = false}):await()
        if err and err.code == 'noHandler' then
            self.results:set('swiftUI', 'skip', name, 'Only Apple platforms open the SwiftUI screen.')
        elseif err and err.code == 'cancelled' then
            self.results:set('swiftUI', 'pass', name, 'The person closed the SwiftUI screen, and the call failed with "cancelled": ' .. err.message)
        elseif err then
            self.results:failure('swiftUI', name, err)
        else
            self.results:set('swiftUI', 'pass', name, string.format('The person %s through %s in %s.', answer.confirmed and 'confirmed' or 'declined', answer.via, answer.language))
        end
    end)
end

-- The answer of a message arrives while the app is still inactive on some platforms, a frame or two before its window has the focus again, and the screen that the answer opens waits until the app is active.
function Screen:openFromDialog()
    self:act(function()
        local name = kRows[8].name
        self.results:set('dialog', 'waiting', name, 'Answer the message, and the screen opens.')
        local button, failure = dialogs.message({title = 'Native screen', text = 'Open the screen of the plugin from the answer of this message?', buttons = {'Open the screen'}}):await()
        if failure then
            self.results:failure('dialog', name, failure)
            return
        elseif not button then
            self.results:set('dialog', 'info', name, 'The message closed without an answer, so no screen opened.')
            return
        end
        local state = haylen.appState()
        local answer, err = demo.openScreen({state = {test = 'screen'}}):await()
        if err and err.code == 'cancelled' then
            self.results:set('dialog', 'pass', name, string.format('The screen opened in the answer of the message, while the app was %s, and the person closed it.', state))
        elseif err then
            self.results:failure('dialog', name, err)
        else
            self.results:set('dialog', 'pass', name, string.format('The screen opened in the answer of the message, while the app was %s, and the person %s.', state, answer.confirmed and 'confirmed' or 'declined'))
        end
    end)
end

-- The state tells which way the screen came back: a restart under the screen or a redirect.
function Screen:showRestored()
    if not restored then
        return
    end
    local row = restored.state and restored.state.redirectedAt and kRows[6] or kRows[5]
    local ending = restored.result and string.format('%s through %s', restored.result.confirmed and 'confirmed' or 'declined', restored.result.via) or string.format('failed with "%s"', tostring(restored.error.code))
    self.results:set(row.key, restored.state and restored.state.test == 'screen' and 'pass' or 'fail', row.name, string.format('The screen "%s" ended after the app started again: %s, with the state %s.', restored.screen, ending, sample.json(restored.state)))
end

-- The engine covers the app before the screen shows, and the listener sees the cover already.
function Screen:covered()
    if self.watch and not self.watch.started then
        self.watch.started = true
        self.watch.covered = haylen.appCovered()
        self.watch.showing = platform.screenShowing()
        self.watch.halted = haylen.halted()
    end
end

function Screen:uncovered()
    local watch = self.watch
    if not watch or not watch.started or watch.ended then
        return
    end
    watch.ended = true
    local passed = watch.covered and watch.showing and watch.halted and watch.renders == 0 and watch.updates == 0
    self.results:set('covered', passed and 'pass' or 'fail', kRows[2].name, string.format('The values of "appCovered" %s, "screenShowing" %s and "halted" %s when "appInactive" arrived, with %d frames drawn and %d updates while the opaque screen showed.', tostring(watch.covered), tostring(watch.showing), tostring(watch.halted), watch.renders, watch.updates))
end

-- Only the frames under the screen count, since on Android the app runs a few frames without the focus between the end of the screen and appActive.
function Screen:underScreen()
    local watch = self.watch
    return watch ~= nil and watch.started and not watch.ended and haylen.appCovered()
end

function Screen:update(dt)
    Screen.super.update(self, dt)
    if self:underScreen() then
        self.watch.updates = self.watch.updates + 1
    end
end

function Screen:render()
    Screen.super.render(self)
    if self:underScreen() then
        self.watch.renders = self.watch.renders + 1
    end
end

return Screen
