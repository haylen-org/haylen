-- Paths of lines, curves and arcs, each card a kind of shape: a star that crosses itself, overlapping rings and a frame with a hole filled by the rule picked, a leaf, a heart, a speech bubble and a pie slice of curves and arcs filled and stroked, a gear kept once and drawn turned and zoomed, and a wave whose commands are given again every frame.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Panels = require('categories.sprites.panels')
local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local Paths = haylen.class('Paths', SpriteTest)

Paths.rules = {{id = 'nonZero', text = 'Non-zero'}, {id = 'evenOdd', text = 'Even-odd'}}
Paths.joins = {{id = 'miter', text = 'Miter'}, {id = 'round', text = 'Round'}, {id = 'bevel', text = 'Bevel'}}
Paths.gearTeeth = 12
Paths.waveSteps = 10

-- The cards of each layout in world units around the center of the view, which the camera fits into the play area.
Paths.layouts = {
    landscape = {view = {1700, 800}, cards = {rules = {-830, -380, 820, 380}, curves = {10, -380, 820, 380}, gear = {-830, 20, 560, 360}, wave = {-250, 20, 1080, 360}}},
    portrait = {view = {900, 1700}, cards = {rules = {-430, -830, 860, 400}, curves = {-430, -410, 860, 400}, gear = {-430, 10, 860, 400}, wave = {-430, 430, 860, 400}}},
}

Paths.code = [[
local star = graphics2d.newPath():moveTo(0, -90):lineTo(53, 73):lineTo(-86, -28):lineTo(86, -28):lineTo(-53, 73):close()
star:fill(x, y, {color = '#FFF2B23A', fillRule = 'evenOdd'})
heart:moveTo(0, 40):cubicTo(-90, -20, -40, -90, 0, -45):cubicTo(40, -90, 90, -20, 0, 40):close()
heart:stroke(x, y, {width = 6, join = 'round', dashes = {14, 8}})
gear:fill(x, y, {rotation = angle, scaleX = zoom, scaleY = zoom})  -- Kept, so turning it never tessellates again.
wave:clear():moveTo(left, top):quadraticTo(cx, cy, x, y)  -- Given again every frame.]]

-- Builds the shapes that stay: the star, the rings and the frame of the rules card, the curves and the gear, each around its own origin.
function Paths:build()
    self.star = graphics2d.newPath()
    for corner = 0, 4 do
        local angle = -math.pi / 2 + corner * 4 * math.pi / 5
        local x, y = math.cos(angle) * 110, math.sin(angle) * 110
        if corner == 0 then
            self.star:moveTo(x, y)
        else
            self.star:lineTo(x, y)
        end
    end
    self.star:close()

    -- Three circles wound the same way overlap, so the nonzero rule fills their overlaps and the even-odd rule cuts them out.
    self.rings = graphics2d.newPath()
    for index = 0, 2 do
        local angle = -math.pi / 2 + index * 2 * math.pi / 3
        self.rings:arc(math.cos(angle) * 42, math.sin(angle) * 42, 62, 0, 2 * math.pi):close()
    end

    -- A rounded frame with a window wound the other way, which both rules cut out.
    self.window = graphics2d.newPath()
    self.window:moveTo(-80, -110):lineTo(80, -110):quadraticTo(110, -110, 110, -80):lineTo(110, 80):quadraticTo(110, 110, 80, 110):lineTo(-80, 110):quadraticTo(-110, 110, -110, 80):lineTo(-110, -80):quadraticTo(-110, -110, -80, -110):close()
    self.window:moveTo(-50, -60):lineTo(-50, 60):lineTo(50, 60):lineTo(50, -60):close()

    self.leaf = graphics2d.newPath():moveTo(-80, 50):quadraticTo(-70, -80, 80, -60):quadraticTo(60, 70, -80, 50):close()
    self.heart = graphics2d.newPath():moveTo(0, 60):cubicTo(-110, -10, -60, -110, 0, -55):cubicTo(60, -110, 110, -10, 0, 60):close()
    self.bubble = graphics2d.newPath()
    self.bubble:moveTo(-70, -60):lineTo(70, -60):quadraticTo(90, -60, 90, -40):lineTo(90, 20):quadraticTo(90, 40, 70, 40):lineTo(-10, 40):lineTo(-50, 80):lineTo(-40, 40):lineTo(-70, 40):quadraticTo(-90, 40, -90, 20):lineTo(-90, -40):quadraticTo(-90, -60, -70, -60):close()
    self.slice = graphics2d.newPath():moveTo(-20, 20):arc(-20, 20, 90, -math.pi / 2, math.pi * 1.4):close()

    -- A gear of teeth around a rim with a hole in its hub, from lines and arcs alone.
    self.gear = graphics2d.newPath()
    local teeth = Paths.gearTeeth
    local step = 2 * math.pi / teeth
    for tooth = 0, teeth - 1 do
        local angle = tooth * step
        self.gear:arc(0, 0, 100, angle, step * 0.45)
        self.gear:arc(0, 0, 128, angle + step * 0.55, step * 0.3)
    end
    self.gear:close()
    self.gear:arc(0, 0, 34, 0, -2 * math.pi):close()
end

function Paths:enter()
    self.rule, self.join, self.dashed, self.zoom = 'evenOdd', 'round', false, 1
    self.time = 0
    self.wave = graphics2d.newPath()
    self:build()
    self.layout = Paths.layouts.landscape
    self:frame{
        code = Paths.code,
        view = self.layout.view,
        hint = 'Pick the fill rule of the first card, how the curves join and dash, and how far the gear zooms.',
        controls = {
            ui.formField{label = 'Fill rule', ui.segmentedControl{id = 'rule', items = Paths.rules, selected = self.rule, onChange = function(event) self.rule = event.value end}},
            ui.formField{label = 'Join', ui.segmentedControl{id = 'join', items = Paths.joins, selected = self.join, onChange = function(event) self.join = event.value end}},
            ui.toggle{id = 'dashed', text = 'Dashes', onChange = function(event) self.dashed = event.checked end},
            ui.formField{label = 'Zoom', ui.slider{id = 'zoom', min = 0.25, max = 2, value = self.zoom, step = 0.05, showValue = true, decimals = 2, onChange = function(event) self.zoom = event.value end}},
        },
        focus = 'rule',
    }
end

-- Picks the layout that draws the cards larger in the play area.
function Paths:resize(area)
    self.layout = Panels.layout(Paths.layouts, area)
    self.view = self.layout.view
end

function Paths:update(dt)
    Paths.super.update(self, dt)
    self.time = self.time + dt
    self:status(string.format('The %s rule fills the first card, and the wave gives its %d curves again every frame.', self.rule == 'evenOdd' and 'even-odd' or 'nonzero', Paths.waveSteps))
end

-- Fills each shape of the card by the rule picked, with its outline over it.
function Paths:drawRules(rect)
    local inner = Panels.card(rect, 'Fill rules')
    local y = inner[2] + inner[4] / 2 - 10
    local scale = math.min(1, inner[4] / 300)
    local shapes = {{path = self.star, color = '#FFF2B23A', caption = 'Crossing'}, {path = self.rings, color = '#FF7FD4FF', caption = 'Overlapping'}, {path = self.window, color = '#FFC9A0FF', caption = 'With a hole'}}
    for index, shape in ipairs(shapes) do
        local x = inner[1] + inner[3] * (index - 0.5) / #shapes
        shape.path:fill(x, y, {color = shape.color, fillRule = self.rule, scaleX = scale, scaleY = scale, layer = 1})
        shape.path:stroke(x, y, {width = 2, color = '#C0E8EAF2', join = 'round', scaleX = scale, scaleY = scale, layer = 2})
        Panels.caption(shape.caption, x, inner[2] + inner[4] - Panels.captionSize - 2)
    end
end

-- Fills and strokes the shapes of curves and arcs with the join and dashes picked.
function Paths:drawCurves(rect)
    local inner = Panels.card(rect, 'Curves and arcs')
    local y = inner[2] + inner[4] / 2 - 14
    local scale = math.min(0.85, inner[3] / 900, inner[4] / 280)
    local stroke = {width = 6, color = '#FFE8EAF2', join = self.join, dashes = self.dashed and {14, 8} or nil, scaleX = scale, scaleY = scale, layer = 2}
    local shapes = {{path = self.leaf, color = '#FF6FDCA0', caption = 'Leaf'}, {path = self.heart, color = '#FFFF8A84', caption = 'Heart'}, {path = self.bubble, color = '#FF8FB0FF', caption = 'Bubble'}, {path = self.slice, color = '#FFF2B23A', caption = 'Slice'}}
    for index, shape in ipairs(shapes) do
        local x = inner[1] + inner[3] * (index - 0.5) / #shapes
        shape.path:fill(x, y, {color = shape.color, scaleX = scale, scaleY = scale, layer = 1})
        shape.path:stroke(x, y, stroke)
        Panels.caption(shape.caption, x, inner[2] + inner[4] - Panels.captionSize - 2)
    end
end

-- The gear keeps its triangles while it turns, and builds them again only when the zoom changes the size of a pixel in its units.
function Paths:drawGear(rect)
    local inner = Panels.card(rect, 'A kept path turned and zoomed')
    local x, y = inner[1] + inner[3] / 2, inner[2] + inner[4] / 2
    local scale = self.zoom * math.min(1, inner[4] / 280)
    graphics2d.pushClip(inner)
    self.gear:fill(x, y, {color = '#FFE0A93B', fillRule = 'evenOdd', rotation = self.time * 0.6, scaleX = scale, scaleY = scale, layer = 1})
    self.gear:stroke(x, y, {width = 4, color = '#FF1E1B33', join = 'round', rotation = self.time * 0.6, scaleX = scale, scaleY = scale, layer = 2})
    graphics2d.popClip()
end

-- The wave gives its commands again every frame, from the left edge of the card to the right, and closes along the bottom.
function Paths:drawWave(rect)
    local inner = Panels.card(rect, 'A path given again every frame')
    local left, right = inner[1] + 10, inner[1] + inner[3] - 10
    local middle, bottom = inner[2] + inner[4] * 0.45, inner[2] + inner[4] - 10
    local width = (right - left) / Paths.waveSteps
    local wave = self.wave:clear():moveTo(left, middle)
    for step = 0, Paths.waveSteps - 1 do
        local x = left + step * width
        wave:quadraticTo(x + width / 2, middle + math.sin(self.time * 2.5 + step * 0.9) * inner[4] * 0.35, x + width, middle)
    end
    wave:lineTo(right, bottom):lineTo(left, bottom):close()
    wave:fill(0, 0, {color = '#FF3A7BD5', layer = 1})
    wave:stroke(0, 0, {width = 5, color = '#FF7FD4FF', join = 'round', layer = 2})
end

function Paths:draw(area)
    local cards = self.layout.cards
    self:drawRules(cards.rules)
    self:drawCurves(cards.curves)
    self:drawGear(cards.gear)
    self:drawWave(cards.wave)
end

return Paths
