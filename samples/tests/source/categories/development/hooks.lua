-- The hook "reloaded" runs on every scene of the stack after a save that reloaded modules, so a scene rebuilds there what it built from code, such as its interface. A module keeps an object with "hotReload.keep", so the connection its new load makes lands on the live object and replaces the old one. The content shows the scoreboard the module titles, with its button and its count, over the values that tell what a save did.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DevelopmentTest = require('categories.development.development-test')
local hud = require('categories.development.edit.hud')

local Hooks = haylen.class('Hooks', DevelopmentTest)

Hooks.code = [[
local hud = hotReload.keep('hud', function()
    return {pressed = signal.new('development.hud'), presses = 0, loads = 0}
end)
function Hooks:reloaded(info)
    self:set('title', {text = hud.title()})  -- The modules that reloaded are in info.modules.
end]]

Hooks.file = 'source/categories/development/edit/hud.lua'
Hooks.steps = {'Change the text that "hud.title" returns.', 'Save the file.', 'The title changes through the "reloaded" hook, "Loads of the module" grows, and one press of "Press" still counts once, because the signal has one listener.'}

function Hooks:init(entry)
    Hooks.super.init(self, entry)
    self.hooks = 0
    self.firstControl = 'press'
    self.shown = {}
end

function Hooks:content()
    return {
        ui.card{gap = 16, align = 'stretch',
            ui.label{text = 'The scoreboard that the module titles', font = 'caption', color = 'textMuted'},
            ui.row{gap = 24, alignItems = 'center',
                ui.label{id = 'title', text = hud.title(), font = 'title', grow = 1},
                ui.label{id = 'presses', text = '', font = 'heading', color = 'accentText'},
                ui.button{id = 'press', text = 'Press', variant = 'primary', onClick = function()
                    hud.pressed:emit()
                end},
            },
        },
        ui.row{wrap = true, gap = 16, alignItems = 'stretch',
            DevelopmentTest.valueCard('hooks', 'Calls of the hook "reloaded"', ''),
            DevelopmentTest.valueCard('loads', 'Loads of the module', ''),
            DevelopmentTest.valueCard('listeners', 'Listeners of the kept signal', ''),
        },
        ui.card{gap = 8, align = 'stretch',
            ui.label{text = 'Modules of the last call of the hook', font = 'caption', color = 'textMuted'},
            ui.label{id = 'modules', text = '', font = 'heading'},
        },
    }
end

-- Runs after every save that reloaded modules in place, with the names and files of the modules.
function Hooks:reloaded(info)
    self.hooks = self.hooks + 1
    self.modules = table.concat(info.modules, ', ')
    self:set('title', {text = hud.title()})
end

-- Shows the values once they change.
function Hooks:update(dt)
    Hooks.super.update(self, dt)
    local values = {
        presses = hud.presses == 1 and 'Pressed once' or string.format('Pressed %d times', hud.presses),
        hooks = tostring(self.hooks),
        loads = tostring(hud.loads),
        listeners = tostring(hud.pressed.size),
        modules = self.modules or 'No module reloaded yet.',
    }
    for id, text in pairs(values) do
        if self.shown[id] ~= text then
            self.shown[id] = text
            self:set(id, {text = text})
        end
    end
end

return Hooks
