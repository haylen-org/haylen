-- The settings sheet, shown over the menu or the paused run. It also shows what the system reports about the device.
local scene = require('haylen.scene')
local system = require('haylen.system')
local ui = require('haylen.ui')
local window = require('haylen.window')

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

    self.gui = ui.mount(ui.column{
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
            ui.column{width = 420, align = 'center', widgets.button('device', 'settings.device', function()
                self:showDevice()
            end, {variant = 'default'})},
            ui.label{id = 'deviceInfo', text = '', color = 'textMuted', textAlign = 'center', align = 'stretch'},
            ui.column{width = 420, align = 'center', widgets.button('back', 'settings.back', function()
                self:close()
            end, {sound = 'back'})},
        },
    })
    self.gui:command('back', 'focus')
end

function settings:showDevice()
    local info = system.info()
    local text = string.format('Device "%s", system "%s" %s, language "%s"', info.deviceModel or info.deviceKind, info.os, info.osVersion or '', system.languages()[1] or '')
    self.gui:set('deviceInfo', {text = text})
end

function settings:close()
    if not self.closing then
        self.closing = true
        preferences.save()
        scene.pop()
    end
end

function settings:exit()
    self.gui:unmount()
end

function settings:update(dt)
    if self.animate then
        self.animate(dt)
    end
end

return settings
