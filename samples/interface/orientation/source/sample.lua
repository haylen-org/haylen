-- What every test of the sample shares: the frame with the Back button, the title, the description and the hints, and the way back to the menu.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local sample = {}

-- The fade that every change between the menu and a test plays.
sample.transition = {effect = 'fade', duration = 0.3, color = '#FF101418'}

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

-- A titled card that groups related values or controls. The children go in the array part of `node`.
function sample.section(title, node)
    table.insert(node, 1, ui.sectionTitle{text = title})
    node.gap = node.gap or 12
    return ui.card(node)
end

-- Whether the visible screen is taller than wide, which a desktop window shows once it is resized that way even though desktops always report landscape.
function sample.tall()
    local visible = viewport.visibleRect()
    return visible.height > visible.width
end

-- The base of every test scene. A test sets `hints` and `focus`, returns its components from `content` and may override `started`, which runs once the document is mounted.
local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

Test.hints = ''
Test.focus = 'back'

function Test:init(entry)
    self.entry = entry
end

function Test:content()
    return ui.spacer{}
end

function Test:started()
end

function Test:enter()
    local entry = self.entry
    -- The Menu button of a TV remote and the Back button of Android reach the test as Escape instead of leaving the app.
    window.setBackLeavesApp(false)
    local body = self:content()
    body.grow = body.grow or 1
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
        ui.label{id = 'status', text = '', color = 'accentText', font = 'monospace', visible = false},
        body,
        ui.label{text = self.hints, font = 'caption', color = 'textMuted'},
    }, {owner = self})
    self.document:command(self.focus, 'focus')
    self:started()
end

-- Shows a line of live values under the description, touching the document only when the text changes.
function Test:setStatus(text)
    if text ~= self.status then
        self.status = text
        self.document:set('status', {text = text, visible = text ~= ''})
    end
end

-- Sets the text of a node, touching the document only when the text changes.
function Test:show(id, text)
    self.shown = self.shown or {}
    if self.shown[id] ~= text then
        self.shown[id] = text
        self.document:set(id, {text = text})
    end
end

return sample
