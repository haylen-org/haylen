-- Fixed steps: the presses of the jump action counted in update, in fixedUpdate through the frame edges and in fixedUpdate through the step edges, at a time scale that makes frames run no fixed step or several, and a hopper whose jump buffer catches a press made just before it lands.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Backdrop = require('categories.animation.backdrop')
local controls = require('categories.input.controls')
local Test = require('harness.test')

local Steps = haylen.class('Steps', Test)

Steps.code = [[
local jump = {name = 'jump', type = 'button', bindings = {'key:space', 'button:south'}, buffer = 0.15}
input.loadActions({actions = {jump}})
if input.stepPressed('jump') then presses = presses + 1 end  -- In `fixedUpdate`, each press counts once.
if grounded and input.consumePress('jump') then velocity = -1100 end  -- A press stays in the buffer for 0.15 seconds.]]

local kScales = {{id = '0.25', text = '0.25'}, {id = '0.5', text = '0.5'}, {id = '1', text = '1'}, {id = '2', text = '2'}, {id = '4', text = '4'}}
local kGravity = 2600
local kJumpSpeed = 1100
local kBuffer = 0.15
-- The walker sheet has cells of 256 pixels in eight columns, with the idle frames first and a frame of the run with the legs apart for the air.
local kIdleFrames = {0, 1, 2, 3}
local kAir = 10
local kFeet = 248 / 256
local kHeroSize = 150

function Steps:enter()
    local actions = controls.actions()
    for _, action in ipairs(actions.actions) do
        if action.name == 'jump' then
            action.buffer = kBuffer
        end
    end
    self:loadActions(actions)
    self.counts = {frame = 0, fixedFrame = 0, fixedStep = 0}
    self.hopper = {y = 0, velocity = 0, jumps = 0}
    self.walker = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.backdrop = Backdrop()
    self.meadowShape = function() graphics2d.drawShape(self.meadow, {radius = 18}) end
    self:frame({
        code = Steps.code,
        hint = 'Press jump many times at each time scale: the step edges count every press once, while frame edges read in fixed steps miss presses below 1 and double them above it.',
        play = true,
        controls = {
            ui.formField{label = 'Time scale', ui.segmentedControl{id = 'scale', items = kScales, selected = '1', onChange = function(event)
                haylen.setTimeScale(tonumber(event.value))
            end}},
            ui.button{id = 'reset', text = 'Reset the counters', onClick = function()
                self.counts = {frame = 0, fixedFrame = 0, fixedStep = 0}
                self.hopper.jumps = 0
            end},
        },
        overlay = {
            ui.touchButton{anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}, action = 'jump', text = 'Jump', touchOnly = true},
        },
    })
end

function Steps:exit()
    Steps.super.exit(self)
    haylen.setTimeScale(1)
    input.clearVirtual()
end

function Steps:update(dt)
    Steps.super.update(self, dt)
    if input.pressed('jump') then
        self.counts.frame = self.counts.frame + 1
    end
    self:status(string.format('Time runs at %.2f times its speed, with fixed steps of %.4f seconds, and jump is %s.', haylen.timeScale(), haylen.fixedStep(), input.down('jump') and 'down' or 'up'))
end

-- The hopper lands every half second or so and jumps again only with a press that the buffer still holds when it touches the ground.
function Steps:fixedUpdate(step)
    if input.pressed('jump') then
        self.counts.fixedFrame = self.counts.fixedFrame + 1
    end
    if input.stepPressed('jump') then
        self.counts.fixedStep = self.counts.fixedStep + 1
    end

    local hopper = self.hopper
    hopper.velocity = hopper.velocity + kGravity * step
    hopper.y = hopper.y + hopper.velocity * step
    if hopper.y >= 0 then
        hopper.y, hopper.velocity = 0, 0
    end
    if hopper.y == 0 and input.consumePress('jump') then
        hopper.velocity = -kJumpSpeed
        hopper.jumps = hopper.jumps + 1
    end
end

function Steps:drawCount(title, value, rect, color)
    graphics2d.drawShape(rect, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    graphics2d.drawText(nil, title, rect[1] + 24, rect[2] + 20, {size = 24, color = Test.muted, maxWidth = rect[3] - 48, layer = 2})
    graphics2d.drawText(nil, tostring(value), rect[1] + 24, rect[2] + rect[4] - 20, {size = 64, color = color, anchor = {0, 1}, layer = 2})
end

function Steps:draw(area)
    local margin, gap = 24, 16
    local width = (area.width - margin * 2 - gap * 2) / 3
    local height = math.min(170, area.height * 0.3)
    self:drawCount('Presses read in "update"', self.counts.frame, {margin, margin, width, height}, Test.ink)
    self:drawCount('Frame edges read in "fixedUpdate"', self.counts.fixedFrame, {margin + width + gap, margin, width, height}, Test.red)
    self:drawCount('Step edges read in "fixedUpdate"', self.counts.fixedStep, {margin + (width + gap) * 2, margin, width, height}, Test.green)

    -- The hopper stands on a meadow under the counters and jumps only with a press that the buffer still holds when it lands.
    local meadow = {margin, margin + height + gap, area.width - margin * 2, area.height - height - gap - margin * 2}
    local ground = meadow[2] + meadow[4] - 40
    self.meadow = meadow
    graphics2d.pushMask(self.meadowShape)
    self.backdrop:draw(graphics2d.canvasBounds(), ground, 150, 0)
    local x = area.width * 0.5
    local frame = self.hopper.y < 0 and kAir or kIdleFrames[math.floor(haylen.elapsed() * 6) % #kIdleFrames + 1]
    Backdrop.shadow(x, ground, 90 + self.hopper.y * 0.1, 3)
    graphics2d.draw(self.walker, x, ground + self.hopper.y, {source = {frame % 8 * 256, frame // 8 * 256, 256, 256}, width = kHeroSize, height = kHeroSize, pivotY = kFeet, layer = 4})
    graphics2d.popMask()
    local text = string.format('Jumps from the buffer of %.2f seconds: %d', kBuffer, self.hopper.jumps)
    local plate = graphics2d.measureText(nil, text, {size = 24}) + 48
    graphics2d.drawShape({meadow[1] + 20, meadow[2] + 20, plate, 48}, {radius = 24, color = '#CC151A2A', layer = 5})
    graphics2d.drawText(nil, text, meadow[1] + 44, meadow[2] + 44, {size = 24, color = Test.ink, anchor = {0, 0.5}, layer = 6})
end

return Steps
