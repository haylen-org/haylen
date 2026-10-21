-- Transparent overlays: a scene with `transparent = true` lets the scenes below keep rendering, while only the top scene updates and receives input. The island below has walkers that move in "update", which stop under an overlay, and clouds and a sun that move in "render", which keep moving.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Card = require('categories.scenes.card')
local SceneTest = require('categories.scenes.scene-test')
local Scenery = require('categories.effects.scenery')

local Overlays = haylen.class('Overlays', SceneTest)

Overlays.code = [[
local Overlay = haylen.class('Overlay', scene.Scene)
function Overlay:init() self.transparent = true end  -- The scenes below keep rendering.
function Overlay:enter() self.gui = ui.mount(ui.card{ui.label{text = 'A dialog'}}, {owner = self}) end
function Overlay:update(dt) if input.pressed('back') then scene.pop() end end  -- Only the top scene updates.
scene.push(Overlay())]]

-- The walker sheet has cells of 256 pixels in eight columns with the run in the second row, and the walkers stand on the ground line of the island.
Overlays.run = {8, 9, 10, 11, 12, 13, 14, 15}
Overlays.ground = 400
Overlays.feet = 248 / 256

-- An overlay that shows a GUI of its own over the play area of the test, a bar along its top like a HUD or a card that dims what is below like a dialog, cascaded by its depth.
local Overlay = haylen.class('Overlay', scene.Scene)

function Overlay:init(depth, kind, stage)
    self.depth = depth
    self.kind = kind
    self.stage = stage
    self.transparent = true
    self.updates = 0
end

function Overlay:enter()
    local safe = viewport.safeRect()
    local offset = (self.depth - 1) * 40
    local margin = {self.stage.y - safe.y + 24 + offset, 0, 0, self.stage.x - safe.x + 24 + offset}
    local text = ui.label{id = 'updates', text = 'Updates while on top: 0', color = 'textMuted'}
    if self.kind == 'hud' then
        self.gui = ui.mount(ui.panel{anchor = 'topLeft', margin = margin, width = 560, gap = 8,
            ui.label{text = 'A HUD overlay at depth ' .. self.depth, font = 'heading'},
            text,
        }, {owner = self, layer = 1})
    else
        self.gui = ui.mount(ui.card{anchor = 'topLeft', margin = {margin[1] + 140, 0, 0, margin[4] + 120}, width = 600, gap = 14,
            ui.label{text = 'A dialog overlay at depth ' .. self.depth, font = 'heading'},
            ui.label{text = 'The island keeps rendering below, and only this overlay updates.', color = 'textMuted'},
            text,
            ui.button{id = 'close', text = 'Close', variant = 'primary', onClick = function() self:close() end},
        }, {owner = self, layer = 1})
    end
end

function Overlay:close()
    if scene.top() == self and not scene.transitioning() then
        scene.pop()
    end
end

function Overlay:update(dt)
    self.updates = self.updates + 1
    self.gui:set('updates', {text = 'Updates while on top: ' .. self.updates})
    if input.pressed('back') then
        self:close()
    end
end

-- A dialog dims the play area under it, and a HUD leaves it as it is.
function Overlay:render()
    if self.kind == 'dialog' then
        graphics2d.beginScreen()
        graphics2d.drawShape(self.stage, {color = '#66000000'})
    end
end

function Overlays:init(entry)
    Overlays.super.init(self, entry)
    self.updates = 0
    self.scenery = Scenery()
    self.walker = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.walkers = {}
    for index = 1, 4 do
        self.walkers[index] = {x = -900 + index * 420, speed = 140 + index * 45, time = index * 0.3}
    end
end

function Overlays:enter()
    self.depth = scene.size()
    self:frame{
        hint = 'Push overlays on the island. The walkers move in "update" and stop under an overlay, while the clouds and the sun move in "render" and keep moving. Escape or the B button pops the top overlay.',
        code = Overlays.code,
        view = Card.world,
        controls = {
            ui.button{id = 'hud', text = 'Push a HUD overlay', align = 'stretch', onClick = function() self:push('hud') end},
            ui.button{id = 'dialog', text = 'Push a dialog overlay', align = 'stretch', onClick = function() self:push('dialog') end},
            ui.button{id = 'pop', text = 'Pop the top overlay', align = 'stretch', onClick = function() self:popTop() end},
        },
        focus = 'hud',
    }
end

function Overlays:push(kind)
    if not scene.transitioning() then
        scene.push(Overlay(scene.size() - self.depth + 1, kind, self.stage))
    end
end

function Overlays:popTop()
    if scene.size() > self.depth and not scene.transitioning() then
        scene.pop()
    end
end

function Overlays:update(dt)
    Overlays.super.update(self, dt)
    self.updates = self.updates + 1
    for _, walker in ipairs(self.walkers) do
        walker.x = walker.x + walker.speed * dt
        walker.time = walker.time + dt
        if walker.x > 1100 then
            walker.x = -1100
        end
    end
end

-- Rendering goes on under the overlays while updates stop, so the status line changes here, where it shows how long the world has been standing still.
function Overlays:renderUi()
    local overlays = scene.size() - self.depth
    local text = string.format('%d %s on the island, which updated %d times.', overlays, overlays == 1 and 'overlay is' or 'overlays are', self.updates)
    if text ~= self.shown then
        self.shown = text
        self:set('status', {text = text})
    end
end

-- The clouds and the sun take their time from the clock in render, and the walkers from their own time in update.
function Overlays:draw(area)
    self.scenery.time = haylen.elapsed()
    self.scenery:draw(false)
    for _, walker in ipairs(self.walkers) do
        local frame = Overlays.run[math.floor(walker.time * 14) % #Overlays.run + 1]
        graphics2d.drawShape({walker.x - 60, Overlays.ground - 10, 120, 22}, {kind = 'ellipse', color = '#40203018', layer = 1})
        graphics2d.draw(self.walker, walker.x, Overlays.ground + 6, {source = {frame % 8 * 256, frame // 8 * 256, 256, 256}, width = 200, height = 200, pivotY = Overlays.feet, layer = 2})
    end
end

return Overlays
