-- What every test of the sample shares: the frame in the middle of the safe area with the Back button, the title, the controls and the hints, the way back to the menu and the insets of the safe area.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local sample = {}

-- The fade that every change between the menu and a test plays.
sample.transition = {effect = 'fade', duration = 0.3, color = '#FF101418'}

-- The simulated devices of `haylen.viewport`, and the device itself.
sample.devices = {
    {id = 'device', text = 'This device'},
    {id = 'iphoneNotch', text = 'iPhone with a notch'},
    {id = 'iphoneDynamicIsland', text = 'iPhone with a dynamic island'},
    {id = 'ipad', text = 'iPad'},
    {id = 'androidGestureBar', text = 'Android with a gesture bar'},
    {id = 'television', text = 'TV'},
}

-- A desktop window and a browser have no notch, so there the sample starts with a simulated iPhone, while phones, tablets and TVs keep their own safe area.
function sample.simulateOnDesktop()
    local platform = haylen.platform
    if platform == 'macos' or platform == 'windows' or platform == 'linux' or platform == 'web' then
        viewport.setSafeAreaSimulation('iphoneDynamicIsland')
    end
end

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

-- Mounts a document that belongs to the test, whose root goes back to the menu when `uiCancel` reaches it.
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

-- Returns how far the safe area stays from each edge of the visible screen, in design units: top, right, bottom and left.
function sample.insets()
    local visible, safe = viewport.visibleRect(), viewport.safeRect()
    return safe.y - visible.y, visible:right() - safe:right(), visible:bottom() - safe:bottom(), safe.x - visible.x
end

-- Fills the four bands of the screen outside the safe area.
function sample.drawBands(color, order)
    local visible, safe = viewport.visibleRect(), viewport.safeRect()
    local top, right, bottom, left = sample.insets()
    graphics2d.drawRect({visible.x, visible.y, visible.width, top}, color, order)
    graphics2d.drawRect({visible.x, safe:bottom(), visible.width, bottom}, color, order)
    graphics2d.drawRect({visible.x, safe.y, left, safe.height}, color, order)
    graphics2d.drawRect({safe:right(), safe.y, right, safe.height}, color, order)
end

-- The base of every test scene. A test sets `hints` and `focus`, returns the controls of the frame from `controls` and may override `started`, which runs once the frame is mounted.
local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

Test.hints = ''
Test.focus = 'back'

function Test:init(entry)
    self.entry = entry
end

function Test:controls()
    return {}
end

function Test:started()
end

-- The frame sits in the middle of the safe area, so the edges stay free for what the test shows.
function Test:enter()
    local entry = self.entry
    window.setBackLeavesApp(false)
    local children = {
        ui.row{gap = 20,
            ui.button{id = 'back', text = 'Back', onClick = function()
                sample.back(entry)
            end},
            ui.label{text = entry.title, font = 'heading'},
        },
        ui.label{text = entry.description, color = 'textMuted'},
        ui.label{id = 'status', text = '', color = 'accentText', font = 'monospace', visible = false},
    }
    for _, control in ipairs(self:controls()) do
        children[#children + 1] = control
    end
    children[#children + 1] = ui.label{text = self.hints, font = 'caption', color = 'textMuted'}
    self.frame = sample.mount(self, ui.card{anchor = 'center', width = 900, gap = 16, children = children}, {layer = 1})
    self.frame:command(self.focus, 'focus')
    self:started()
end

function Test:setStatus(text)
    if text ~= self.status then
        self.status = text
        self.frame:set('status', {text = text, visible = text ~= ''})
    end
end

return sample
