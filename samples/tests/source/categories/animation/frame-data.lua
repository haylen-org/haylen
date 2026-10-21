-- Frame data: a boxer punches from an atlas whose slices give every frame a body box, a hand point and the pivot of the feet, and the punch frame a hitbox, drawn over him as he flips, turns and plays faster or slower. A training dummy stands within reach on each side and rocks when the hitbox lands on it. The events of the frames scroll in a list, and the same punch rebuilt with frames that last ticks steps on a fixed step of a thirtieth of a second.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AnimationTest = require('categories.animation.animation-test')
local Backdrop = require('categories.animation.backdrop')

local FrameData = haylen.class('FrameData', AnimationTest)

FrameData.world = {960, 540}
FrameData.ground = 175
FrameData.scale = 0.84
-- The dummies stand this far to each side of the fighter, where the glove of the punch lands on their sacks.
FrameData.reach = 262
FrameData.lines = 6
FrameData.ticks = {4, 4, 6, 8}
FrameData.tick = 1 / 30
FrameData.names = {byTime = 'by time', byTicks = 'by ticks'}
FrameData.ink = '#FF1E1B33'
FrameData.legend = {{'Body box', '#FF2F86F6'}, {'Hitbox', '#FFF0473E'}, {'Hand', '#FFFFC93C'}, {'Pivot', '#FF4CD45A'}}

FrameData.code = [[
animator:add('byTime', atlas:animation('punch'))
animator:add('byTicks', animation2d.fromFrames(atlas.texture, frames))  -- Frames of 4, 4, 6 and 8 ticks.
animator.onEvent = function(name, event, frame) self:record(name, event, frame) end
local hit = animator:box('hit', fighter)  -- The hitbox in the world, or nil.
local x, y = animator:point('hand', fighter)  -- Follows flips, turns and scale.
animator:step(1)  -- One tick of the fixed step.]]

-- Rebuilds the punch of the atlas with frames that last ticks, keeping the data of every frame.
function FrameData.tickPunch(atlas)
    local frames = {}
    for index, name in ipairs({'punch 0', 'punch 1', 'punch 2', 'punch 3'}) do
        local data = atlas:frame(name)
        frames[index] = {source = data.source, pivot = data.pivot, ticks = FrameData.ticks[index], boxes = data.boxes, points = data.points, events = data.events}
    end
    local punch = animation2d.fromFrames(atlas.texture, frames)
    punch:addEvent(1, 'ready')
    return punch
end

function FrameData:enter()
    local atlas = AnimationTest.atlas()
    self.backdrop = Backdrop()
    self.history = {}
    self.ticked = false
    self.flipped = false
    self.turning = false
    self.time = 0
    self.tickTime = 0
    self.hits = 0
    self.fighter = graphics2d.newSprite(atlas.texture, {y = FrameData.ground, scaleX = FrameData.scale, scaleY = FrameData.scale})
    self.dummyTexture = assets.texture('animation/dummy.png', AnimationTest.linear)
    self.dummies = {}
    for side = -1, 1, 2 do
        self.dummies[#self.dummies + 1] = {x = side * FrameData.reach, side = side, angle = 0, spin = 0, flash = 0}
    end
    self.animator = animation2d.newAnimator()
    local byTime = atlas:animation('punch')
    byTime:addEvent(1, 'ready')
    self.animator:add('byTime', byTime)
    self.animator:add('byTicks', FrameData.tickPunch(atlas))
    self.animator.onEvent = function(name, event, frame) self:record(name, event, frame) end
    self.animator:play('byTime')

    self:frame{
        code = FrameData.code,
        hint = 'The punch lands on the dummy the fighter faces. Step by ticks plays the punch on a fixed step of 30 ticks a second.',
        controls = {
            ui.toggle{id = 'flip', text = 'Face left', onChange = function(event) self.flipped = event.checked end},
            ui.toggle{id = 'turn', text = 'Turn the fighter', onChange = function(event) self.turning = event.checked end},
            ui.toggle{id = 'ticks', text = 'Step by ticks', onChange = function(event)
                self.ticked = event.checked
                self.tickTime = 0
                self.animator:play(event.checked and 'byTicks' or 'byTime', true)
            end},
            ui.formField{label = 'Playback speed', ui.slider{id = 'speed', min = 0.1, max = 2, value = 1, step = 0.1, decimals = 1, showValue = true, onChange = function(event) self.animator.speed = event.value end}},
            ui.sectionTitle{text = 'Frame events'},
            ui.column{id = 'history', gap = 4, ui.label{text = 'The events of the frames show here as the punch plays.', color = 'textMuted'}},
        },
        focus = 'flip',
    }
end

function FrameData:record(name, event, frame)
    table.insert(self.history, 1, string.format('%s%s on frame %d, %s', event:sub(1, 1):upper(), event:sub(2), frame, FrameData.names[name]))
    self.history[FrameData.lines + 1] = nil
    local rows = {}
    for index, text in ipairs(self.history) do
        rows[index] = ui.label{text = text, color = index == 1 and 'text' or 'textMuted'}
    end
    self.gui:replaceChildren('history', rows)
end

-- The tick version advances on a fixed step of a thirtieth of a second, the way a game on fixed ticks plays its frames.
function FrameData:fixedUpdate(step)
    if not self.ticked then
        return
    end
    self.tickTime = self.tickTime + step * self.animator.speed
    while self.tickTime >= FrameData.tick do
        self.tickTime = self.tickTime - FrameData.tick
        self.animator:step(1)
    end
end

function FrameData:update(dt)
    FrameData.super.update(self, dt)
    if not self.area then
        return
    end
    Backdrop.frame(self, FrameData.world, FrameData.ground, 0.8)
    self.time = self.time + dt
    if not self.ticked then
        self.animator:update(dt)
    end
    self.fighter.flipHorizontal = self.flipped
    self.fighter.rotation = self.turning and math.sin(self.time * 1.4) * 0.3 or 0
    self.animator:apply(self.fighter)

    -- A hitbox that reaches a dummy rocks it once per punch, away from the fighter.
    local hit = self.animator:box('hit', self.fighter)
    for _, dummy in ipairs(self.dummies) do
        local body = self:dummyBox(dummy)
        local touching = hit ~= nil and hit.x < body.x + body.width and body.x < hit.x + hit.width and hit.y < body.y + body.height and body.y < hit.y + hit.height
        if touching and not dummy.touched then
            dummy.spin = dummy.spin + dummy.side * 3.2
            dummy.flash = 0.1
            self.hits = self.hits + 1
        end
        dummy.touched = touching
        dummy.spin = dummy.spin + (-dummy.angle * 60 - dummy.spin * 7) * dt
        dummy.angle = dummy.angle + dummy.spin * dt
        dummy.flash = math.max(0, dummy.flash - dt)
    end

    local frame = self.animator.frame
    local name = FrameData.names[self.animator.current]
    if hit then
        self:status(string.format('Frame %d of 4 %s, with a hitbox of %.0f by %.0f. Punches landed: %d.', frame, name, hit.width, hit.height, self.hits))
    else
        self:status(string.format('Frame %d of 4 %s, without a hitbox. Punches landed: %d.', frame, name, self.hits))
    end
end

-- The sack of a dummy, which the hitbox has to reach, in the world.
function FrameData:dummyBox(dummy)
    local width, height = self.dummyTexture.width * FrameData.scale, self.dummyTexture.height * FrameData.scale
    return m.rect(dummy.x - width * 0.42, FrameData.ground - height * 0.86, width * 0.84, height * 0.5)
end

function FrameData:draw(area)
    local bounds = graphics2d.canvasBounds()
    self.backdrop:draw(bounds, FrameData.ground, 300)
    for _, dummy in ipairs(self.dummies) do
        Backdrop.shadow(dummy.x, FrameData.ground, 150, 0)
        graphics2d.draw(self.dummyTexture, dummy.x, FrameData.ground + 4, {pivotY = 0.99, scaleX = FrameData.scale, scaleY = FrameData.scale, rotation = dummy.angle, flash = dummy.flash > 0 and '#60FFFFFF' or nil, layer = 1})
    end
    Backdrop.shadow(self.fighter.x, FrameData.ground, 260, 0)
    self.fighter:draw()
    self:drawData()
end

-- The frame data drawn over the fighter, each mark with a dark rim so it reads over the sky and the grass.
function FrameData:drawData()
    local body = self.animator:box('body', self.fighter)
    if body then
        FrameData.outline(body, FrameData.legend[1][2])
    end
    local hit = self.animator:box('hit', self.fighter)
    if hit then
        FrameData.outline(hit, FrameData.legend[2][2])
    end
    local x, y = self.animator:point('hand', self.fighter)
    if x then
        graphics2d.drawShape({x - 9, y - 9, 18, 18}, {kind = 'ellipse', color = FrameData.legend[3][2], borderWidth = 3, borderColor = FrameData.ink, layer = 6})
    end
    local px, py = self.fighter.x, self.fighter.y
    graphics2d.drawShape({px - 15, py - 4, 30, 8}, {radius = 4, color = FrameData.legend[4][2], borderWidth = 2, borderColor = FrameData.ink, layer = 6})
    graphics2d.drawShape({px - 4, py - 15, 8, 30}, {radius = 4, color = FrameData.legend[4][2], borderWidth = 2, borderColor = FrameData.ink, layer = 6})
end

function FrameData.outline(rect, color)
    graphics2d.drawShape(rect, {radius = 6, color = '#00000000', borderWidth = 6, borderColor = FrameData.ink, layer = 5})
    graphics2d.drawShape({rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4}, {radius = 4, color = '#00000000', borderWidth = 3, borderColor = color, layer = 6})
end

-- The legend of the marks sits in the top left corner of the play area, at the size of the interface.
function FrameData:renderUi()
    if not self.stage then
        return
    end
    graphics2d.beginScreen()
    local x, y = self.stage.x + 16, self.stage.y + 16
    local width = 0
    for _, entry in ipairs(FrameData.legend) do
        width = width + 28 + graphics2d.measureText(nil, entry[1], {size = 24}) + 20
    end
    graphics2d.drawShape({x, y, width + 4, 48}, {radius = 12, color = '#D91E2138', layer = 10})
    x = x + 16
    for _, entry in ipairs(FrameData.legend) do
        graphics2d.drawShape({x, y + 15, 18, 18}, {radius = 5, color = entry[2], borderWidth = 2, borderColor = FrameData.ink, layer = 11})
        graphics2d.drawText(nil, entry[1], x + 28, y + 24, {size = 24, color = '#FFF2F3FF', anchor = {0, 0.5}, layer = 11})
        x = x + 28 + graphics2d.measureText(nil, entry[1], {size = 24}) + 20
    end
end

return FrameData
