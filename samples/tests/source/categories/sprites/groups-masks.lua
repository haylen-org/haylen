-- Groups and masks: a round portrait and a shaped card drawn through masks with smooth edges, fog of war over a small island that a soft hole cuts open around the pointer with an outside mask, and the same overlapping sprites faded one by one beside a group that fades them as one image.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Panels = require('categories.sprites.panels')
local SpriteTest = require('categories.sprites.sprite-test')
local Terrain = require('categories.sprites.terrain')
local Test = require('harness.test')

local GroupsMasks = haylen.class('GroupsMasks', SpriteTest)

GroupsMasks.propOptions = {filter = 'linear', mipmaps = true}
GroupsMasks.columns = 12
GroupsMasks.rows = 10
-- The props of the island from the back to the front: a tile of the terrain image or a tall tree of the props image, its place as a share of the map and its height in cells.
GroupsMasks.props = {
    {image = 'terrain', source = {398, 266, 128, 128}, x = 0.6, y = 0.15, size = 0.9},
    {image = 'tree', source = {134, 2, 128, 256}, x = 0.92, y = 0.22, size = 2.3},
    {image = 'tree', source = {2, 2, 128, 256}, x = 0.12, y = 0.3, size = 2.2},
    {image = 'terrain', source = {926, 266, 128, 128}, x = 0.32, y = 0.36, size = 0.9},
    {image = 'terrain', source = {662, 266, 128, 128}, x = 0.16, y = 0.7, size = 0.8},
    {image = 'terrain', source = {266, 266, 128, 128}, x = 0.38, y = 0.88, size = 0.9},
    {image = 'terrain', source = {530, 398, 128, 128}, x = 0.6, y = 0.9, size = 0.6},
    {image = 'tree', source = {2, 2, 128, 256}, x = 0.88, y = 0.95, size = 2},
}
GroupsMasks.layouts = {
    landscape = {view = {1700, 800}, cards = {portrait = {-830, -380, 400, 370}, card = {-830, 10, 400, 370}, fog = {-410, -380, 800, 760}, groups = {410, -380, 420, 760}}},
    portrait = {view = {900, 1700}, cards = {portrait = {-430, -830, 420, 400}, card = {10, -830, 420, 400}, fog = {-430, -410, 860, 760}, groups = {-430, 370, 860, 460}}},
}

GroupsMasks.code = [[
graphics2d.pushMask(function() graphics2d.drawCircle(x, y, 90, '#FFFFFFFF') end)
graphics2d.draw(hero, x, y)  -- Shows only inside the circle.
graphics2d.popMask()
graphics2d.pushMask(function() graphics2d.drawShape(hole, {radius = 130, softness = 90}) end, {mode = 'outside'})
graphics2d.beginGroup({opacity = 0.5})  -- Overlaps fade as one image.]]

function GroupsMasks:enter()
    self.hero = SpriteTest.texture('images/hero.png')
    self.tree = SpriteTest.texture('images/tree.png')
    self.rock = SpriteTest.texture('images/rock.png')
    self.images = {tree = assets.texture('tiled/tiles/props.png', GroupsMasks.propOptions), terrain = assets.texture('tiled/tiles/terrain.png', GroupsMasks.propOptions)}
    local sky = graphics.newImage(4, 128)
    sky:fillGradient({angle = math.pi / 2, stops = {{0, '#FF4A90D9'}, {0.75, '#FF9ED3F0'}, {1, '#FFF6E2A8'}}})
    self.sky = graphics.newTexture(sky, {filter = 'linear'})
    self.opacity = 0.5
    self.time = 0
    self.layout = GroupsMasks.layouts.landscape
    self:frame{
        code = GroupsMasks.code,
        view = self.layout.view,
        hint = 'Move the pointer over the island to light the fog around it, and change the opacity of the faded sprites.',
        controls = {
            ui.formField{label = 'Opacity of the faded sprites', ui.slider{id = 'opacity', min = 0, max = 1, value = self.opacity, step = 0.05, showValue = true, onChange = function(event) self.opacity = event.value end}},
        },
        focus = 'opacity',
    }
end

-- Picks the layout that draws the cards larger in the play area and bakes the island of its fog card: grass around a pond with a sandy shore and a dirt path that reaches it.
function GroupsMasks:resize(area)
    local layout = Panels.layout(GroupsMasks.layouts, area)
    self.view = layout.view
    if layout == self.layout and self.grass then
        return
    end
    self.layout = layout
    local rect = layout.cards.fog
    local columns, rows = GroupsMasks.columns, GroupsMasks.rows
    local size = math.min((rect[3] - 40) / columns, (rect[4] - 106) / rows)
    self.map = {rect[1] + (rect[3] - columns * size) / 2, rect[2] + 66, columns * size, rows * size}
    self.cell = size
    self.grass, self.edges = Terrain.bake(columns, rows, size, self.map[1], self.map[2], function(column, row)
        local dx, dy = (column - 8) / 2.2, (row - 5) / 2
        local pond = dx * dx + dy * dy
        if pond < 1 then
            return Terrain.water
        end
        if pond < 1.9 then
            return Terrain.sand
        end
        if (row == 5 or row == 6) and column <= 5 then
            return Terrain.path
        end
        return Terrain.grass
    end)
end

function GroupsMasks:update(dt)
    GroupsMasks.super.update(self, dt)
    self.time = self.time + dt
    self.pointerX, self.pointerY = self:pointer()
    local stats = graphics2d.stats()
    self:status(string.format('The masks and groups draw through %d canvases in %d passes and %d draw calls.', stats.canvases, stats.passes, stats.drawCalls))
end

-- The hero in a round frame whose mask breathes, over a sky and a strip of grass that the circle cuts.
function GroupsMasks:drawPortrait(rect)
    local inner = Panels.card(rect, 'Round portrait')
    local x, y = inner[1] + inner[3] / 2, inner[2] + inner[4] / 2
    local radius = math.min(inner[3], inner[4]) * 0.44 + math.sin(self.time * 2) * 6
    graphics2d.pushMask(function()
        graphics2d.drawCircle(x, y, radius, '#FFFFFFFF')
    end)
    graphics2d.drawShape({x - 140, y - 140, 280, 280}, {texture = self.sky})
    graphics2d.draw(self.images.terrain, x, y + 70, {source = {2, 2, 128, 128}, width = 300, height = 150, pivotY = 0})
    graphics2d.draw(self.hero, x, y + 96, {scaleX = 200 / self.hero.height, scaleY = 200 / self.hero.height, pivotY = 1, layer = 1})
    graphics2d.popMask()
    graphics2d.drawRing(x, y, radius + 4, 8, '#FFFFC93C', {layer = 2})
    graphics2d.drawRing(x, y, radius + 9, 3, '#FF1E1B33', {layer = 2})
end

-- A collectible card: its picture reaches past the card on every side, and the mask with rounded corners keeps it inside, under a gold border and a name plate.
function GroupsMasks:drawCard(rect)
    local inner = Panels.card(rect, 'Shaped card')
    local height = inner[4]
    local width = math.min(inner[3], height * 0.72)
    local x, y = inner[1] + (inner[3] - width) / 2, inner[2]
    local shape = {x, y, width, height}
    graphics2d.drawShape({x + 6, y + 10, width, height}, {radius = 26, color = '#70000000', softness = 14, layer = -2})
    graphics2d.pushMask(function()
        graphics2d.drawShape(shape, {radius = 26})
    end)
    graphics2d.drawShape({x - 40, y - 40, width + 80, height + 80}, {texture = self.sky})
    for column = -1, 2 do
        graphics2d.draw(self.images.terrain, x + column * width * 0.4, y + height * 0.66, {source = {2, 2, 128, 128}, width = width * 0.4, height = width * 0.4, pivotX = 0, pivotY = 0})
        graphics2d.draw(self.images.terrain, x + column * width * 0.4, y + height * 0.66 + width * 0.4, {source = {2, 2, 128, 128}, width = width * 0.4, height = width * 0.4, pivotX = 0, pivotY = 0})
    end
    graphics2d.draw(self.images.tree, x - width * 0.08, y + height * 0.74, {source = {2, 2, 128, 256}, width = width * 0.5, height = width, pivotY = 1, layer = 1})
    graphics2d.draw(self.hero, x + width * 0.62, y + height * 0.8, {scaleX = width * 0.66 / self.hero.height, scaleY = width * 0.66 / self.hero.height, pivotY = 1, layer = 2})
    graphics2d.drawShape({x, y + height - 54, width, 54}, {color = '#E01E1B33', layer = 3})
    graphics2d.drawText(nil, 'Island knight', x + width / 2, y + height - 27, {size = 26, color = '#FFFFE9A8', anchor = {0.5, 0.5}, layer = 4})
    graphics2d.popMask()
    graphics2d.drawShape(shape, {radius = 26, color = '#00000000', borderWidth = 6, borderColor = '#FFFFC93C', layer = 1})
end

-- The island under its fog, whose hole follows the pointer over the map and wanders around the pond while the pointer is elsewhere.
function GroupsMasks:drawFog(rect)
    Panels.card(rect, 'Fog of war around the pointer')
    local map, cell = self.map, self.cell
    local holeX, holeY
    local pointerX, pointerY = self.pointerX or -1e9, self.pointerY or -1e9
    if pointerX >= map[1] and pointerX <= map[1] + map[3] and pointerY >= map[2] and pointerY <= map[2] + map[4] then
        holeX, holeY = pointerX, pointerY
    else
        holeX = map[1] + map[3] * (0.5 + math.sin(self.time * 0.5) * 0.3)
        holeY = map[2] + map[4] * (0.5 + math.sin(self.time * 0.8) * 0.28)
    end

    graphics2d.pushMask(function()
        graphics2d.drawShape(map, {radius = 14})
    end)
    graphics2d.drawStatic(self.grass, 0, 0)
    graphics2d.drawStatic(self.edges, 0, 0, {layer = 1})
    for _, prop in ipairs(GroupsMasks.props) do
        local height = cell * prop.size
        local width = height * prop.source[3] / prop.source[4]
        local x, y = map[1] + map[3] * prop.x, map[2] + map[4] * prop.y
        graphics2d.draw(self.images[prop.image], x, y, {source = prop.source, width = width, height = height, pivotY = 0.92, layer = 2})
    end
    graphics2d.pushMask(function()
        graphics2d.drawShape({holeX - 130, holeY - 130, 260, 260}, {radius = 130, softness = 90})
    end, {mode = 'outside', layer = 10})
    graphics2d.drawRect(map, '#EE141A2A')
    graphics2d.popMask()
    graphics2d.popMask()
end

-- A tree, the hero and a rock overlap over a patch of sky, drawn once with each sprite faded by itself, which shows the tree through the hero, and once inside a group that fades the three as one picture.
function GroupsMasks:sprites(x, y, size, alpha)
    local color = string.format('#%02XFFFFFF', math.floor(alpha * 255 + 0.5))
    local tree, hero, rock = size / self.tree.height, size * 0.72 / self.hero.height, size * 0.33 / self.rock.height
    graphics2d.draw(self.tree, x - size * 0.14, y - size * 0.04, {scaleX = tree, scaleY = tree, pivotY = 1, color = color})
    graphics2d.draw(self.hero, x + size * 0.1, y, {scaleX = hero, scaleY = hero, pivotY = 1, color = color, layer = 1})
    graphics2d.draw(self.rock, x - size * 0.3, y + 4, {scaleX = rock, scaleY = rock, pivotY = 1, color = color, layer = 2})
end

function GroupsMasks:drawGroups(rect)
    local inner = Panels.card(rect, 'Fading sprites')
    local vertical = inner[4] > inner[3]
    local half = vertical and inner[4] / 2 or inner[3] / 2
    local size = math.min((vertical and half or inner[4]) - 60, (vertical and inner[3] or half) * 0.78) / 1.14
    for index, name in ipairs({'One by one', 'As one group'}) do
        local x = vertical and inner[1] + inner[3] / 2 or inner[1] + half * (index - 0.5)
        local feet = vertical and inner[2] + half * index - 48 - size * 0.08 or inner[2] + inner[4] - 48 - size * 0.08
        graphics2d.drawShape({x - size * 0.62, feet - size * 1.06, size * 1.24, size * 1.14}, {radius = 16, texture = self.sky, layer = -2})
        if index == 1 then
            self:sprites(x, feet, size, self.opacity)
        else
            graphics2d.beginGroup({opacity = self.opacity})
            self:sprites(x, feet, size, 1)
            graphics2d.endGroup()
        end
        Panels.caption(name, x, feet + size * 0.08 + 10)
    end
end

function GroupsMasks:draw(area)
    local cards = self.layout.cards
    self:drawPortrait(cards.portrait)
    self:drawCard(cards.card)
    self:drawFog(cards.fog)
    self:drawGroups(cards.groups)
end

return GroupsMasks
