-- Behind the trees: four ways to show a character who walks behind something, side by side in four panels of one meadow, or one at a time on a narrow screen. Fade lowers the opacity of the tree in front, Dither turns the tree around the character into a pattern through a mask with a `dither`, Cutout opens a round hole in the tree through an outside mask, and Silhouette draws the hidden part of the character over the tree as a flat color through a mask of the tree. The character walks a loop around the trees on its own, or where the player moves it.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local ParticleTest = require('categories.particles.particle-test')
local Test = require('harness.test')

local BehindTrees = haylen.class('BehindTrees', Test)

BehindTrees.code = [[
graphics2d.draw(oak, x, y, {color = {1, 1, 1, behind and 0.35 or 1}})  -- Fade
graphics2d.pushMask(function() graphics2d.drawShape({x - r, y - r, r * 2, r * 2}, {radius = r, softness = r}) end, {mode = 'outside', dither = 6})  -- Dither
graphics2d.pushMask(function() graphics2d.drawCircle(hero.x, hero.y - 90, 110, '#FFFFFFFF') end, {mode = 'outside'})  -- Cutout
graphics2d.pushMask(function() graphics2d.draw(oak, x, y) end, {layer = 5})  -- Silhouette: only over the tree
graphics2d.draw(walker, hero.x, hero.y, {source = frame, flash = '#FF8FD3FF'})
graphics2d.popMask()]]

BehindTrees.actions = {actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left'}},
    {name = 'point', type = 'button', bindings = {'mouse:left'}},
}}
BehindTrees.techniques = {
    {id = 'fade', text = 'Fade'},
    {id = 'dither', text = 'Dither'},
    {id = 'cutout', text = 'Cutout'},
    {id = 'silhouette', text = 'Silhouette'},
}

-- Each panel shows the same meadow of 960 by 720 around a point a little below the origin, which the panels of a narrow screen cannot show side by side.
BehindTrees.view = {960, 720}
BehindTrees.narrowWidth = 1200
BehindTrees.frameSize = 256
BehindTrees.heroSize = 190
BehindTrees.speed = 220
BehindTrees.silhouette = '#FF8FD3FF'
BehindTrees.trees = {
    {image = 'effects/oak.png', x = -150, y = 170, height = 470},
    {image = 'effects/chestnut.png', x = 260, y = 110, height = 410},
}

function BehindTrees:init(entry)
    BehindTrees.super.init(self, entry)
    self.walker = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.tiles = assets.texture(ParticleTest.tiles, {filter = 'linear'})
    self.trees = {}
    for index, tree in ipairs(BehindTrees.trees) do
        local texture = assets.texture(tree.image, {filter = 'linear'})
        local width = tree.height * texture.width / texture.height
        self.trees[index] = {texture = texture, x = tree.x, y = tree.y, width = width, height = tree.height, canopy = m.rect(tree.x - width / 2, tree.y - tree.height, width, tree.height * 0.78), reveal = 0}
    end
    self.cameras = {}
    for index = 1, #BehindTrees.techniques do
        self.cameras[index] = graphics2d.newCamera()
    end
    self.hero = {x = -420, y = 40, facingLeft = false, moving = false, time = 0, loop = 0}
    self.auto = true
    self.dither = 6
    self.shown = 'dither'
end

function BehindTrees:enter()
    self:loadActions(BehindTrees.actions)
    local choices = {}
    for index, technique in ipairs(BehindTrees.techniques) do
        choices[index] = {id = technique.id, text = technique.text}
    end
    self:frame{
        hint = 'Move the character with the arrows, WASD, a stick or a press on the meadow. It walks a loop around the trees on its own until you move it.',
        code = BehindTrees.code,
        play = true,
        controls = {
            ui.toggle{id = 'auto', text = 'Walk on its own', checked = self.auto, onChange = function(event) self.auto = event.checked end},
            ui.formField{label = 'Dither cell', ui.slider{id = 'dither', min = 2, max = 16, value = self.dither, showValue = true, decimals = 0, onChange = function(event) self.dither = event.value end}},
            ui.formField{label = 'Panel on a narrow screen', ui.segmentedControl{id = 'shown', items = choices, selected = self.shown, onChange = function(event) self.shown = event.value end}},
        },
    }
end

-- The loop of the character passes behind both trees and in front of them, on an ellipse around the middle of the meadow.
function BehindTrees:walkOnItsOwn(dt)
    local hero = self.hero
    hero.loop = hero.loop + dt * 0.45
    local x, y = math.cos(hero.loop) * 380 + 40, math.sin(hero.loop) * 170 + 110
    return x - hero.x, y - hero.y
end

function BehindTrees:steer(dt)
    local dx, dy = input.vector('move')
    if dx ~= 0 or dy ~= 0 then
        self.auto = false
        self:set('auto', {checked = false})
        return dx * BehindTrees.speed * dt, dy * BehindTrees.speed * dt
    end
    local target = self:pressedPoint()
    if target then
        self.target = target
        self.auto = false
        self:set('auto', {checked = false})
    end
    if self.auto then
        return self:walkOnItsOwn(dt)
    end
    if not self.target then
        return 0, 0
    end
    local hero = self.hero
    local tx, ty = self.target.x - hero.x, self.target.y - hero.y
    local distance = math.sqrt(tx * tx + ty * ty)
    if distance < 4 then
        self.target = nil
        return 0, 0
    end
    local step = math.min(distance, BehindTrees.speed * dt)
    return tx / distance * step, ty / distance * step
end

-- The meadow point under a press of the mouse or a finger, in the panel it lands in.
function BehindTrees:pressedPoint()
    local touch = input.touches()[1]
    local x, y
    if touch then
        x, y = touch.x, touch.y
    elseif input.down('point') then
        x, y = input.mousePosition()
    else
        return nil
    end
    for index, camera in ipairs(self.cameras) do
        local view = camera.viewport
        if view and x >= view.x and y >= view.y and x < view.x + view.width and y < view.y + view.height and self:panelShows(index) then
            local wx, wy = camera:screenToWorld(x, y)
            return {x = m.clamp(wx, -440, 440), y = m.clamp(wy, -40, 300)}
        end
    end
    return nil
end

function BehindTrees:update(dt)
    BehindTrees.super.update(self, dt)
    if not self.stage then
        return
    end
    self:placePanels()

    local hero = self.hero
    local dx, dy = self:steer(dt)
    hero.x, hero.y = m.clamp(hero.x + dx, -440, 440), m.clamp(hero.y + dy, -40, 300)
    hero.moving = math.abs(dx) + math.abs(dy) > 0.01
    if math.abs(dx) > 0.01 then
        hero.facingLeft = dx < 0
    end
    hero.time = hero.time + dt

    -- A tree hides the character while the feet stand behind its foot and the body reaches into its crown, and its fade and reveal ease toward that.
    local body = m.rect(hero.x - 50, hero.y - BehindTrees.heroSize * 0.9, 100, BehindTrees.heroSize * 0.9)
    local hidden = 0
    for _, tree in ipairs(self.trees) do
        tree.behind = hero.y < tree.y and tree.canopy:intersects(body)
        local goal = tree.behind and 1 or 0
        tree.reveal = tree.reveal + (goal - tree.reveal) * math.min(1, dt * 8)
        hidden = hidden + (tree.behind and 1 or 0)
    end
    self:status(hidden > 0 and 'The character is behind a tree, which each panel shows in its own way.' or 'The character is in the open, and every panel draws the trees as they are.')
end

-- The four panels share the play area two by two, or one fills it on a narrow screen, each fitting the meadow into its part.
function BehindTrees:placePanels()
    local stage = self.stage
    local narrow = stage.width < BehindTrees.narrowWidth
    for index, camera in ipairs(self.cameras) do
        local view
        if narrow then
            view = self:panelShows(index) and m.rect(stage.x, stage.y, stage.width, stage.height) or nil
        else
            local column, row = (index - 1) % 2, (index - 1) // 2
            local gap = 8
            local width, height = (stage.width - gap) / 2, (stage.height - gap) / 2
            view = m.rect(stage.x + column * (width + gap), stage.y + row * (height + gap), width, height)
        end
        camera.viewport = view
        if view then
            local zoom = math.min(view.width / BehindTrees.view[1], view.height / BehindTrees.view[2])
            camera.zoom = {zoom, zoom}
            camera.position = {0, 20}
        end
    end
    self.narrow = narrow
end

function BehindTrees:panelShows(index)
    return not self.narrow or BehindTrees.techniques[index].id == self.shown
end

-- Paints the screen behind the frame like every test, then each panel as a world canvas sorted by the y its draws stand on, in the part of the play area its camera covers.
function BehindTrees:render()
    graphics2d.beginScreen({order = -1})
    graphics2d.drawRect(viewport.visibleRect(), ui.themeColor('background'))
    if not self.stage then
        return
    end
    for index, technique in ipairs(BehindTrees.techniques) do
        local camera = self.cameras[index]
        if camera.viewport then
            graphics2d.beginWorld(camera, {sort = 'y'})
            self:drawMeadow()
            self:drawTrees(technique.id)
            self:drawHero(technique.id)
            ParticleTest.legend({{text = technique.text}})
        end
    end
end

function BehindTrees:drawMeadow()
    local area = graphics2d.canvasBounds()
    local size = 160
    for row = math.floor(area.y / size) * size, area:bottom(), size do
        for column = math.floor(area.x / size) * size, area:right(), size do
            local flowers = (math.floor(column / size) * 7 + math.floor(row / size) * 3) % 5 == 0
            graphics2d.draw(self.tiles, column, row, {source = flowers and ParticleTest.floors.flowers or ParticleTest.floors.grass, width = size, height = size, pivotX = 0, pivotY = 0, layer = -10})
        end
    end
end

function BehindTrees:heroFrame()
    local hero = self.hero
    local frame = hero.moving and 9 + math.floor(hero.time * 12) % 8 or 1 + math.floor(hero.time * 6) % 4
    local size = BehindTrees.frameSize
    return {(frame - 1) % 8 * size, (frame - 1) // 8 * size, size, size}
end

function BehindTrees:drawHero(technique, options)
    local hero = self.hero
    options = options or {}
    graphics2d.draw(self.walker, hero.x, hero.y, {source = self:heroFrame(), width = BehindTrees.heroSize, height = BehindTrees.heroSize, pivotX = 0.5, pivotY = 0.94, flipHorizontal = hero.facingLeft, flash = options.flash, layer = options.layer})
    if technique ~= 'silhouette' or options.flash then
        return
    end

    -- The hidden part of the character draws over each tree that hides it as a flat color, through a mask of that tree.
    for _, tree in ipairs(self.trees) do
        if tree.behind then
            graphics2d.pushMask(function() self:drawTree(tree, {}) end, {layer = 5})
            self:drawHero(technique, {flash = BehindTrees.silhouette})
            graphics2d.popMask()
        end
    end
end

function BehindTrees:drawTree(tree, options)
    graphics2d.draw(tree.texture, tree.x, tree.y, {width = tree.width, height = tree.height, pivotY = 1, color = options.color, layer = options.layer})
end

-- The trees show the character behind them in the way of the panel, eased in and out as the character goes behind them and leaves. The image of a mask stands on the bottom of the canvas, so its sort offset moves it back to the foot of its tree.
function BehindTrees:drawTrees(technique)
    local hero = self.hero
    local bottom = graphics2d.canvasBounds():bottom()
    for _, tree in ipairs(self.trees) do
        local reveal = tree.reveal
        if technique == 'fade' then
            self:drawTree(tree, {color = {1, 1, 1, 1 - 0.65 * reveal}})
        elseif technique == 'dither' and reveal > 0.01 then
            local radius = 60 + 70 * reveal
            graphics2d.pushMask(function()
                graphics2d.drawShape({hero.x - radius, hero.y - 100 - radius, radius * 2, radius * 2}, {radius = radius, softness = radius})
            end, {mode = 'outside', dither = self.dither, sortOffset = tree.y - bottom})
            self:drawTree(tree, {})
            graphics2d.popMask()
        elseif technique == 'cutout' and reveal > 0.01 then
            graphics2d.pushMask(function()
                graphics2d.drawCircle(hero.x, hero.y - 95, 115 * reveal, '#FFFFFFFF')
            end, {mode = 'outside', sortOffset = tree.y - bottom})
            self:drawTree(tree, {})
            graphics2d.popMask()
        else
            self:drawTree(tree, {})
        end
    end
end

return BehindTrees
