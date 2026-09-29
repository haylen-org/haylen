-- What every test of the sample shares: the base scene with the header and the Back button, the cursor the player steers and the way back to the menu.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local sample = {}

-- The fade that every change between the menu and a test plays.
sample.transition = {effect = 'fade', duration = 0.35, color = '#FF101418'}

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

-- Tells whether the player pressed the place action this frame, with its keys, buttons or a click, or tapped outside the interface.
function sample.pressed()
    if input.pressed('place') then
        return true
    end
    local touch = input.touches()[1]
    return touch ~= nil and touch.phase == 'began' and not ui.wantsPointer()
end

-- The base of every test scene. A test sets hints, may return controls for the panel on the right and calls the methods it overrides here. The header belongs to the scene, so it goes away when the scene unloads.
local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

Test.hints = ''

function Test:init(entry)
    self.entry = entry
end

function Test:controls()
    return nil
end

function Test:enter()
    local entry = self.entry
    -- The Menu button of a TV remote and the Back button of Android reach the test as Escape instead of leaving the app.
    window.setBackLeavesApp(false)
    local top = ui.row{
        gap = 16,
        ui.button{id = 'back', text = 'Back', align = 'start', onClick = function()
            sample.back(entry)
        end},
        ui.panel{
            width = 760,
            gap = 4,
            align = 'start',
            ui.label{text = entry.title, font = 'heading'},
            ui.label{text = entry.description, color = 'textMuted'},
            ui.label{id = 'status', text = '', color = 'accentText', visible = false},
        },
        ui.spacer{grow = 1},
    }
    local controls = self:controls()
    if controls then
        top[#top + 1] = ui.panel{id = 'controls', width = 460, gap = 12, align = 'start', children = controls}
    end
    self.header = ui.mount(ui.column{
        padding = 24,
        gap = 16,
        top,
        ui.spacer{grow = 1},
        ui.label{text = self.hints, font = 'body', color = 'text', outline = '#FF000000', outlineWidth = 3},
    }, {owner = self})
end

function Test:update(dt)
    if input.pressed('back') then
        sample.back(self.entry)
    end
end

-- Shows a line of live values under the description, touching the document only when the text changes.
function Test:setStatus(text)
    if text ~= self.status then
        self.status = text
        self.header:set('status', {text = text, visible = text ~= ''})
    end
end

-- A point on the screen that follows the mouse and the first finger and moves with WASD and the left stick, so every device can aim.
local Cursor = haylen.class('Cursor')
sample.Cursor = Cursor

function Cursor:init(x, y)
    local area = viewport.visibleRect()
    self.x = x or area.x + area.width / 2
    self.y = y or area.y + area.height / 2
    self.speed = 900
end

function Cursor:update(dt)
    local touch = input.touches()[1]
    local mouseX, mouseY = input.mouseDelta()
    if touch and not ui.wantsPointer() then
        self.x, self.y = touch.x, touch.y
    elseif mouseX ~= 0 or mouseY ~= 0 then
        self.x, self.y = input.mousePosition()
    end

    local moveX, moveY = input.vector('move')
    local area = viewport.visibleRect()
    self.x = math.max(area.x, math.min(area.x + area.width, self.x + moveX * self.speed * dt))
    self.y = math.max(area.y, math.min(area.y + area.height, self.y + moveY * self.speed * dt))
end

-- Returns the cursor in the world of a camera.
function Cursor:world(camera)
    return camera:screenToWorld(self.x, self.y)
end

-- Draws the cursor in a screen canvas.
function Cursor:draw()
    graphics2d.drawRing(self.x, self.y, 14, 3, '#C0FFFFFF', {layer = 100})
    graphics2d.drawCircle(self.x, self.y, 3, '#FFFFFFFF', {layer = 100})
end

return sample
