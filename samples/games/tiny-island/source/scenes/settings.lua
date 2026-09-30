-- The settings sheet, shown over the menu or the paused run. It also shows what the system reports about the device and tries the platform bridge with the Google sign-in of the local plugin google-sign-in.
local scene = require('haylen.scene')
local system = require('haylen.system')
local ui = require('haylen.ui')
local window = require('haylen.window')

local googleSignIn = require('google-sign-in')
local preferences = require('systems.preferences')
local sound = require('systems.sound')
local widgets = require('ui.widgets')

local settings = {}
settings.__index = settings

local languages = {{id = 'en', text = 'English'}, {id = 'pt', text = 'Português'}}

-- Takes an optional function that keeps the screen below animated, since only the top scene updates.
function settings.new(animate)
    return setmetatable({animate = animate, transparent = true}, settings)
end

function settings:enter()
    window.setBackLeavesApp(false)
    local rows = {
        ui.settingsRow{label = widgets.text('settings.music'), ui.slider{value = preferences.get('music'), width = 440, onChange = function(event)
            preferences.set('music', event.value)
        end}},
        ui.settingsRow{label = widgets.text('settings.effects'), ui.slider{value = preferences.get('effects'), width = 440, onChange = function(event)
            preferences.set('effects', event.value)
        end}},
        ui.settingsRow{label = widgets.text('settings.language'), ui.combo{items = languages, selected = preferences.get('language'), width = 440, onChange = function(event)
            preferences.set('language', event.value)
        end}},
        ui.settingsRow{label = widgets.text('settings.touch'), ui.toggle{checked = preferences.get('touch'), onChange = function(event)
            preferences.set('touch', event.checked)
        end}},
    }
    if preferences.desktop() then
        rows[#rows + 1] = ui.settingsRow{label = widgets.text('settings.fullscreen'), ui.toggle{checked = preferences.get('fullscreen'), onChange = function(event)
            preferences.set('fullscreen', event.checked)
        end}}
    end

    local bridge = {widgets.button('device', 'settings.device', function()
        local info = system.info()
        self.document:set('bridge', {text = string.format('Device "%s", system "%s" %s, locale "%s"', info.deviceModel or info.deviceKind, info.os, info.osVersion or '', info.locale or '')})
    end, {variant = 'default', grow = 1})}
    if googleSignIn.available then
        bridge[2] = widgets.button('google', 'settings.google', function()
            self:ask(googleSignIn.signIn, function(account)
                return widgets.text('settings.signedIn', {name = account.name or account.email})
            end)
        end, {variant = 'default', grow = 1})
    end

    self.document = ui.mount(ui.column{
        justify = 'center',
        padding = 48,
        onCancel = function()
            sound.play('back')
            self:close()
        end,
        ui.panel{
            width = 1040,
            align = 'center',
            gap = 18,
            ui.label{text = widgets.text('settings.title'), font = 'title', textAlign = 'center', align = 'center'},
            ui.settingsForm{children = rows},
            ui.row{gap = 24, children = bridge},
            ui.label{id = 'bridge', text = '', color = 'textMuted', textAlign = 'center', align = 'stretch'},
            ui.column{width = 420, align = 'center', widgets.button('back', 'settings.back', function()
                self:close()
            end, {sound = 'back'})},
        },
    })
    self.document:command('back', 'focus')
end

-- Starts a platform call and shows what it answered, or why it failed, under the buttons. The task belongs to the sheet, so an answer that comes after the sheet closed goes nowhere.
function settings:ask(start, describe)
    scene.spawn(self, function()
        local result, err = start():await()
        if not result then
            sound.play('error')
        end
        self.document:set('bridge', {text = result and describe(result) or err.message})
    end)
end

function settings:close()
    if not self.closing then
        self.closing = true
        preferences.save()
        scene.pop()
    end
end

function settings:exit()
    self.document:unmount()
end

function settings:update(dt)
    if self.animate then
        self.animate(dt)
    end
end

return settings
