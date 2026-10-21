-- Zoom around a point: `camera:zoomAt` keeps the world point under a screen point in place, which is how the wheel and a pinch zoom toward the pointer, within the zoom limits.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Zoom = haylen.class('Zoom', CameraTest)

Zoom.code = [[
camera.minZoom = 0.25
camera.maxZoom = 4
local _, wheel = input.mouseScroll()
local x, y = input.mousePosition()
camera:zoomAt(1.15 ^ wheel, x, y)  -- The world point under the pointer stays in place.]]

Zoom.home = {x = -820, y = -180}

function Zoom:init(entry)
    Zoom.super.init(self, entry)
    self.world = World()
    -- The walker waits by the cottage as a measure of the size of things at every zoom.
    self.walker = Walker(World.cottage.x + 160, World.cottage.y + 150)
    self.camera = graphics2d.newCamera()
    self.camera.minZoom = 0.25
    self.camera.maxZoom = 4
    self.camera.position = {Zoom.home.x, Zoom.home.y}
    self.shown = 1
end

function Zoom:enter()
    local camera = self.camera
    self:loadActions(actions)
    self:frame{
        hint = 'Zoom with the wheel or a pinch toward the pointer, with Z and X, minus and plus or the shoulders and triggers toward the middle, or with the slider. Pan with WASD, the arrows, the left stick or by dragging.',
        code = Zoom.code,
        play = true,
        controls = {
            ui.formField{label = 'Zoom', ui.slider{id = 'zoom', min = camera.minZoom, max = camera.maxZoom, value = 1, showValue = true, decimals = 2, onChange = function(event)
                local stage = self.stage
                camera:zoomAt(event.value / camera.zoom.x, stage.x + stage.width / 2, stage.y + stage.height / 2)
                self.shown = camera.zoom.x
            end}},
            ui.button{id = 'reset', text = 'Reset the view', onClick = function()
                camera.zoom = {1, 1}
                camera.position = {Zoom.home.x, Zoom.home.y}
            end},
        },
    }
end

function Zoom:update(dt)
    Zoom.super.update(self, dt)
    local stage = self.stage
    if not stage then
        return
    end
    local camera = self.camera
    local mouseX, mouseY = input.mousePosition()
    local _, wheel = input.mouseScroll()
    if wheel ~= 0 and stage:contains({mouseX, mouseY}) and not ui.usingPointer() then
        camera:zoomAt(1.15 ^ wheel, mouseX, mouseY)
    end

    -- A pinch reports its spread since the second finger landed, so each frame zooms by the change since the last one.
    local pinched = false
    for _, gesture in ipairs(input.gestures()) do
        if gesture.type == 'pinch' and stage:contains({gesture.x, gesture.y}) then
            camera:zoomAt(gesture.scale / (self.pinch or gesture.scale), gesture.x, gesture.y)
            self.pinch = gesture.scale
            pinched = true
        end
    end
    if not pinched and #input.touches() < 2 then
        self.pinch = nil
    end

    local keys = input.value('zoom')
    if keys ~= 0 then
        camera:zoomAt(1 + keys * dt * 1.5, stage.x + stage.width / 2, stage.y + stage.height / 2)
    end

    local moveX, moveY = input.vector('move')
    local dragX, dragY = 0, 0
    local touches = input.touches()
    if input.down('point') and #touches == 0 and stage:contains({mouseX, mouseY}) then
        dragX, dragY = input.mouseDelta()
    elseif #touches == 1 and not ui.usingPointer() and stage:contains({touches[1].x, touches[1].y}) then
        dragX, dragY = touches[1].dx, touches[1].dy
    end
    camera.x = camera.x + moveX * 700 * dt / camera.zoom.x - dragX / camera.zoom.x
    camera.y = camera.y + moveY * 700 * dt / camera.zoom.y - dragY / camera.zoom.y
    self.walker:moveTo(self.walker.x, self.walker.y, dt)

    if math.abs(camera.zoom.x - self.shown) > 0.005 then
        self.shown = camera.zoom.x
        self:set('zoom', {value = self.shown})
    end
    local zoom = camera.zoom.x
    if zoom <= camera.minZoom + 0.001 then
        self:status('The view is zoomed out as far as its smallest zoom allows.')
    elseif zoom >= camera.maxZoom - 0.001 then
        self:status('The view is zoomed in as far as its largest zoom allows.')
    else
        self:status(string.format('Things show at %d percent of their size.', math.floor(zoom * 100 + 0.5)))
    end
end

function Zoom:draw(area)
    self.world:draw()
    self.walker:draw()
end

return Zoom
