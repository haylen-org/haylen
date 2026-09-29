-- What every test of the sample shares: the frame with the Back button, the title, the description, a panel of controls and the hints around the stage the test draws in, and the way back to the menu.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = {}

-- The fade that every change between the menu and a test plays.
sample.transition = {effect = 'fade', duration = 0.3, color = '#FF101418'}

-- The colors the tests draw their captions and guides with.
sample.muted = '#FFA3A8BF'
sample.guide = '#806A7090'

function sample.open(entry)
    if not scene.transitioning() then
        scene.replace(require(entry.module)(entry), sample.transition)
    end
end

-- Returns to the menu with the button of the test focused.
function sample.back(entry)
    if not scene.transitioning() then
        scene.replace(require('scenes.menu')(entry.id), sample.transition)
    end
end

-- Draws a caption in the default font, such as the size of the line next to it.
function sample.caption(text, x, y, options)
    local style = {size = 22, color = sample.muted}
    for key, value in pairs(options or {}) do
        style[key] = value
    end
    graphics2d.drawText(nil, text, x, y, style)
end

-- The base of every test scene. A test sets `hints` and `focus`, may return controls for the panel on the right from `controls` and draws in `self:stage()`.
local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

Test.hints = ''
Test.focus = 'back'

function Test:init(entry)
    self.entry = entry
end

function Test:controls()
    return nil
end

function Test:started()
end

function Test:enter()
    local entry = self.entry
    -- The Menu button of a TV remote and the Back button of Android reach the test as Escape instead of leaving the app.
    window.setBackLeavesApp(false)
    local middle = {ui.spacer{id = 'stage', grow = 1, align = 'stretch'}}
    local controls = self:controls()
    if controls then
        middle[2] = ui.panel{width = 440, gap = 16, align = 'start', children = controls}
    end
    self.document = ui.mount(ui.column{
        padding = {24, 32},
        gap = 12,
        onCancel = function()
            sample.back(entry)
        end,
        ui.row{gap = 24,
            ui.button{id = 'back', text = 'Back', onClick = function()
                sample.back(entry)
            end},
            ui.label{text = entry.title, font = 'heading'},
        },
        ui.label{text = entry.description, color = 'textMuted'},
        ui.label{id = 'status', text = '', color = 'accentText', visible = false},
        ui.row{grow = 1, gap = 24, children = middle},
        ui.label{text = self.hints, font = 'caption', color = 'textMuted'},
    }, {owner = self})
    self.document:command(self.focus, 'focus')
    self:started()
end

-- The part of the screen the frame leaves to the test, known once the frame has been drawn.
function Test:stage()
    return self.document:bounds('stage')
end

-- Shows a line of live values under the description, touching the document only when the text changes.
function Test:setStatus(text)
    if text ~= self.status then
        self.status = text
        self.document:set('status', {text = text, visible = text ~= ''})
    end
end

return sample
