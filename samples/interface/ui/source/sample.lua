-- What every test of the sample shares: the frame with the Back button, the title, the description and the hints, and the way back to the menu.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
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

-- Mounts a document that belongs to the test. Its root goes back to the menu when uiCancel reaches it, which Escape, the east button, the Back button of a gamepad, the Menu button of an Apple TV remote and the Back button of Android send once no popup, dialog or focus scope takes it.
function sample.mount(test, tree, options)
    tree.onCancel = tree.onCancel or function()
        sample.back(test.entry)
    end
    local settings = {owner = test}
    for key, value in pairs(options or {}) do
        settings[key] = value
    end
    return ui.mount(tree, settings)
end

-- A row of columns across the test area, each starting at the top unless it sets its own `align`.
function sample.columns(node)
    for _, child in ipairs(node) do
        child.align = child.align or 'start'
    end
    node.gap = node.gap or 24
    return ui.row(node)
end

-- A card that groups the components of one kind under a title. The children go in the array part of `node`.
function sample.section(title, node)
    table.insert(node, 1, ui.sectionTitle{text = title})
    node.gap = node.gap or 12
    return ui.card(node)
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
    self.document = sample.mount(self, ui.column{
        padding = {24, 32},
        gap = 16,
        ui.row{
            gap = 24,
            align = 'stretch',
            ui.button{id = 'back', text = 'Back', align = 'start', onClick = function()
                sample.back(entry)
            end},
            ui.column{
                grow = 1,
                gap = 4,
                ui.label{text = entry.title, font = 'heading'},
                ui.label{text = entry.description, color = 'textMuted'},
                ui.label{id = 'status', text = '', color = 'accentText', visible = false},
            },
        },
        body,
        ui.label{text = self.hints, font = 'caption', color = 'textMuted'},
    })
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

return sample
