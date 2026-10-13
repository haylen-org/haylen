-- Fixed steps: the presses of the jump action counted in update, in fixedUpdate through the frame edges and in fixedUpdate through the step edges, at a time scale that makes frames run no fixed step or several, and a hopper whose jump buffer catches a press made just before it lands.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local controls = require('categories.input.controls')
local Test = require('harness.test')

local Steps = haylen.class('Steps', Test)

local kScales = {{id = '0.25', text = '0.25'}, {id = '0.5', text = '0.5'}, {id = '1', text = '1'}, {id = '2', text = '2'}, {id = '4', text = '4'}}
local kGravity = 2600
local kJumpSpeed = 1100
local kBuffer = 0.15

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
    self:frame({
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
            ui.sectionTitle{text = 'Reading presses in fixed steps'},
            ui.label{font = 'monospace', text = "input.stepPressed('jump')\ninput.stepDown('jump')\ninput.consumePress('jump')"},
        },
        overlay = {
            ui.touchButton{anchor = 'bottomRight', margin = {0, 40, 110, 0}, action = 'jump', text = 'Jump'},
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
    self:status(string.format('Time scale %.2f, fixed step %.4f s, jump %s', haylen.timeScale(), haylen.fixedStep(), input.down('jump') and 'down' or 'up'))
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

function Steps:drawCount(title, value, x, y, color)
    graphics2d.drawRect({x, y, 300, 150}, Test.surface)
    Test.caption(title, x + 20, y + 16, {maxWidth = 260})
    Test.caption(tostring(value), x + 20, y + 70, {size = 56, color = color})
end

function Steps:draw(area)
    self:drawCount('Presses read in update', self.counts.frame, 24, 24, Test.ink)
    self:drawCount('Frame edges read in fixedUpdate', self.counts.fixedFrame, 340, 24, Test.red)
    self:drawCount('Step edges read in fixedUpdate', self.counts.fixedStep, 656, 24, Test.green)

    local ground = area.height - 80
    local x = area.width * 0.5
    graphics2d.drawRect({24, ground, area.width - 48, 4}, Test.line)
    graphics2d.drawCircle(x, ground - 40 + self.hopper.y, 40, Test.warm, {layer = 1})
    Test.caption(string.format('Jumps from the buffer of %.2f s: %d', kBuffer, self.hopper.jumps), 24, ground + 16, {color = Test.ink})
end

return Steps
