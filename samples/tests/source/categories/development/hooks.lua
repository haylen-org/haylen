-- The hook "reloaded" runs on every scene of the stack after a save that reloaded modules, so a scene rebuilds there what it built from code, such as its interface. A module keeps an object with "hotReload.keep", so the connection its new load makes lands on the live object and replaces the old one.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DevelopmentTest = require('categories.development.development-test')
local Test = require('harness.test')
local hud = require('categories.development.edit.hud')

local Hooks = haylen.class('Hooks', DevelopmentTest)

Hooks.file = 'source/categories/development/edit/hud.lua'
Hooks.steps = {'Change the text that "hud.title" returns.', 'Save the file.', 'The title changes through the "reloaded" hook, "Loads of the module" grows, and one press of "Press" still counts once, because the signal has one listener.'}

function Hooks:init(entry)
    Hooks.super.init(self, entry)
    self.hooks = 0
    self.modules = 'none yet'
    self.firstControl = 'press'
end

function Hooks:controls()
    return {ui.label{id = 'title', text = hud.title(), font = 'heading'}, ui.button{id = 'press', text = 'Press', variant = 'primary', onClick = function()
        hud.pressed:emit()
    end}}
end

-- Runs after every save that reloaded modules in place, with the names and files of the modules.
function Hooks:reloaded(info)
    self.hooks = self.hooks + 1
    self.modules = table.concat(info.modules, ', ')
    self:set('title', {text = hud.title()})
end

function Hooks:draw(area)
    local rows = {
        {'Calls of the hook "reloaded"', tostring(self.hooks)},
        {'Modules of the last call', self.modules},
        {'Loads of the module', tostring(hud.loads)},
        {'Listeners of the kept signal', tostring(hud.pressed.size)},
        {'Presses counted', tostring(hud.presses)},
    }
    for index, row in ipairs(rows) do
        local y = 40 + (index - 1) * 80
        Test.caption(row[1], 60, y, {size = 26, color = Test.muted})
        Test.caption(row[2], 60, y + 34, {size = 28, color = Test.ink})
    end
end

return Hooks
