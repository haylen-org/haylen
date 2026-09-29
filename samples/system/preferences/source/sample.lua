-- What every test of the sample shares: the page with the Back button, the title, the description and the hint line, the way to and from the menu, and the way values show as JSON.
local haylen = require('haylen')
local json = require('json')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = {}

-- The fade that every change between the menu and a test plays.
sample.transition = {effect = 'fade', duration = 0.3, color = '#FF101418'}

function sample.open(entry)
    if not scene.transitioning() then
        scene.push(require(entry.module)(entry), sample.transition)
    end
end

function sample.back()
    if not scene.transitioning() then
        scene.pop(sample.transition)
    end
end

-- Formats a Lua value as indented JSON, the way preferences.json holds it.
function sample.json(value)
    if value == nil then
        return 'nil'
    end
    return json.encode(value, {pretty = true})
end

-- The base of every test scene. A test sets its hints and the control that takes the focus, and returns the nodes of its page from content.
local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

Test.hints = ''
Test.focus = 'back'

function Test:init(entry)
    self.entry = entry
end

function Test:content()
    return {}
end

-- Mounts the page, which the scene owns, so it goes away when the scene unloads. The Back button, Escape, the east gamepad button and the Menu button of a TV remote return to the menu.
function Test:enter()
    window.setBackLeavesApp(false)
    self.document = ui.mount(ui.column{
        padding = 24,
        gap = 16,
        onCancel = sample.back,
        ui.row{gap = 24,
            ui.button{id = 'back', text = 'Back', align = 'start', onClick = sample.back},
            ui.column{grow = 1, gap = 4,
                ui.label{text = self.entry.title, font = 'heading'},
                ui.label{text = self.entry.description, color = 'textMuted'},
            },
        },
        ui.row{height = 0, grow = 1, gap = 24, children = self:content()},
        ui.label{text = self.hints, font = 'caption', color = 'textMuted'},
    }, {owner = self})
    self.document:command(self.focus, 'focus')
end

function Test:exit()
    window.setBackLeavesApp(true)
end

function Test:show(id, properties)
    self.document:set(id, properties)
end

return sample
