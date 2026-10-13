-- Frame data: a fighter punches from an atlas whose slices give every frame a hitbox, a hand point and the pivot of the feet, drawn over the fighter as it flips, turns and grows. The events of the frames scroll in a list, and the same punch rebuilt with frames that last ticks steps on the fixed step.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local AnimationTest = require('categories.animation.animation-test')
local Test = require('harness.test')

local FrameData = haylen.class('FrameData', AnimationTest)

FrameData.lines = 6
FrameData.ticks = {4, 4, 6, 8}

FrameData.code = [[
animator.onEvent = function(name, event, frame) log.info(event) end
local hit = animator:box('hit', fighter)  -- The hitbox in the world, or nil.
local x, y = animator:point('hand', fighter)  -- Follows flips, turns and scale.
animator:step(1)  -- Frames that count ticks end exactly on fixed steps.]]

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
    self.history = {}
    self.ticked = false
    self.flipped = false
    self.turning = false
    self.time = 0
    self.fighter = graphics2d.newSprite(atlas.texture, {scaleX = 2.5, scaleY = 2.5})
    self.animator = animation2d.newAnimator()
    local seconds = atlas:animation('punch')
    seconds:addEvent(1, 'ready')
    self.animator:add('seconds', seconds)
    self.animator:add('ticks', FrameData.tickPunch(atlas))
    self.animator.onEvent = function(name, event, frame) self:record(string.format('Event "%s" at frame %d of "%s"', event, frame, name)) end
    self.animator:play('seconds')

    self:frame{
        code = FrameData.code,
        hint = 'The blue box is the body, the red box the hitbox of the third frame, the yellow dot the hand and the cross the pivot on the feet.',
        controls = {
            ui.toggle{id = 'flip', text = 'Face left', onChange = function(event) self.flipped = event.checked end},
            ui.toggle{id = 'turn', text = 'Turn the fighter', onChange = function(event) self.turning = event.checked end},
            ui.toggle{id = 'ticks', text = 'Step by ticks at 30 per second', onChange = function(event)
                self.ticked = event.checked
                self.animator:play(event.checked and 'ticks' or 'seconds', true)
            end},
            ui.formField{label = 'Speed', ui.slider{id = 'speed', min = 0.1, max = 2, value = 1, step = 0.1, showValue = true, onChange = function(event) self.animator.speed = event.value end}},
            ui.column{id = 'history', gap = 4},
        },
        focus = 'flip',
    }
end

function FrameData:record(line)
    table.insert(self.history, 1, line)
    self.history[FrameData.lines + 1] = nil
    local rows = {}
    for index, text in ipairs(self.history) do
        rows[index] = ui.label{text = text, font = 'caption', color = index == 1 and 'text' or 'textMuted'}
    end
    self.gui:replaceChildren('history', rows)
end

-- The tick version advances on a fixed step of a thirtieth of a second, the way a game on fixed ticks plays its frames.
function FrameData:fixedUpdate(step)
    if not self.ticked then
        return
    end
    self.tickTime = (self.tickTime or 0) + step
    while self.tickTime >= 1 / 30 do
        self.tickTime = self.tickTime - 1 / 30
        self.animator:step(1)
    end
end

function FrameData:update(dt)
    FrameData.super.update(self, dt)
    if not self.area then
        return
    end
    self.time = self.time + dt
    if not self.ticked then
        self.animator:update(dt)
    end
    self.fighter.flipHorizontal = self.flipped
    self.fighter.rotation = self.turning and math.sin(self.time) * 0.6 or 0
    self.fighter.x = self.area.width * 0.5
    self.fighter.y = self.area.height * 0.75
    self.animator:apply(self.fighter)
    local hit = self.animator:box('hit', self.fighter)
    self:status(string.format('Animation %s   Frame %d   Time %.2f   Hitbox %s', self.animator.current, self.animator.frame, self.animator.time, hit and string.format('%.0f x %.0f', hit.width, hit.height) or 'none'))
end

function FrameData:draw(area)
    graphics2d.drawRect({0, self.fighter.y, area.width, 4}, '#FF2C3147')
    self.fighter:draw()
    local body = self.animator:box('body', self.fighter)
    if body then
        graphics2d.drawRectOutline(body, 2, Test.accent, {layer = 5})
    end
    local hit = self.animator:box('hit', self.fighter)
    if hit then
        graphics2d.drawRectOutline(hit, 3, Test.red, {layer = 5})
    end
    local x, y = self.animator:point('hand', self.fighter)
    if x then
        graphics2d.drawCircle(x, y, 7, Test.warm, {layer = 5})
    end
    graphics2d.drawLine(self.fighter.x - 12, self.fighter.y, self.fighter.x + 12, self.fighter.y, 2, Test.green, {layer = 5})
    graphics2d.drawLine(self.fighter.x, self.fighter.y - 12, self.fighter.x, self.fighter.y + 12, 2, Test.green, {layer = 5})
end

return FrameData
