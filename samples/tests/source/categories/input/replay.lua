-- Replay: a hopper that moves and jumps on fixed steps while a recording keeps its buttons and the state of the generator of its sparks, then plays the run back in place of the devices, with the hash of each step that shows the replay matches the run.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Replay = haylen.class('Replay', Test)

Replay.code = [[
local recorder = input.newRecorder({buttons = {'left', 'right', 'jump'}, data = random:state()})
recorder:write(1, input.actions())  -- Each fixed step records the buttons of player 1.
recorder:advance(hash)
local playback = input.newPlayback(input.loadRecording(recorder:bytes()))
local mask = playback:mask(1)  -- Bit 1 is `left`, bit 2 is `right` and bit 3 is `jump`.
playback:advance()]]

local kButtons = {'left', 'right', 'jump'}
local kActions = {actions = {
    {name = 'left', type = 'button', bindings = {'key:a', 'key:left', 'button:dpadLeft', 'axis:leftX-', 'virtual:left'}},
    {name = 'right', type = 'button', bindings = {'key:d', 'key:right', 'button:dpadRight', 'axis:leftX+', 'virtual:right'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'key:w', 'button:south', 'virtual:jump'}},
}}
local kSpeed = 360
local kGravity = 2400
local kJump = 900
local kLimit = 60 * 20

function Replay:enter()
    self:loadActions(kActions)
    self:startRecording()
    self:frame({
        code = Replay.code,
        hint = 'Move and jump to record a run of up to 20 seconds, then replay it: the hopper and its sparks repeat every step.',
        play = true,
        controls = {
            ui.button{id = 'record', text = 'Record again', onClick = function() self:startRecording() end},
            ui.button{id = 'replay', text = 'Replay the run', onClick = function() self:startReplay() end},
        },
        overlay = {
            ui.row{anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}, gap = 16,
                ui.touchButton{action = 'left', text = 'Left', size = 100, touchOnly = true},
                ui.touchButton{action = 'right', text = 'Right', size = 100, touchOnly = true},
                ui.touchButton{action = 'jump', text = 'Jump', size = 100, touchOnly = true},
            },
        },
    })
end

function Replay:exit()
    Replay.super.exit(self)
    input.clearVirtual()
end

function Replay:resetWorld(randomState)
    self.hopper = {x = 0, y = 0, vy = 0}
    self.sparks = {}
    self.random = m.random(1)
    self.random:setState(randomState)
end

function Replay:startRecording()
    local seed = m.random(os.time()):state()
    self.recorder = input.newRecorder({buttons = kButtons, data = seed})
    self.playback = nil
    self.mismatches = 0
    self:resetWorld(seed)
end

function Replay:startReplay()
    self.playback = input.newPlayback(input.loadRecording(self.recorder:bytes()))
    self.mismatches = 0
    self:resetWorld(self.playback.data)
end

-- The hash of a step folds the position of the hopper and the count of its sparks into 32 bits.
function Replay:hash()
    local hopper = self.hopper
    return (math.floor(hopper.x * 100) * 31 + math.floor(hopper.y * 100) * 17 + #self.sparks) & 0xFFFFFFFF
end

function Replay:fixedUpdate(step)
    local mask
    if self.playback then
        if self.playback.finished then
            return
        end
        mask = self.playback:mask(1)
    elseif self.recorder.steps < kLimit then
        self.recorder:write(1, input.actions())
        mask = input.actions():stepButtons(kButtons)
    else
        return
    end

    local hopper = self.hopper
    local direction = (mask & 2 ~= 0 and 1 or 0) - (mask & 1 ~= 0 and 1 or 0)
    hopper.x = math.max(-600, math.min(600, hopper.x + direction * kSpeed * step))
    hopper.vy = hopper.vy + kGravity * step
    hopper.y = math.min(0, hopper.y + hopper.vy * step)
    if hopper.y == 0 then
        hopper.vy = 0
        if mask & 4 ~= 0 then
            hopper.vy = -kJump
            for _ = 1, 6 do
                self.sparks[#self.sparks + 1] = {x = hopper.x + self.random:range(-40, 40), y = self.random:range(-30, 0), life = 1}
            end
        end
    end
    for index = #self.sparks, 1, -1 do
        local spark = self.sparks[index]
        spark.y = spark.y - 120 * step
        spark.life = spark.life - step
        if spark.life <= 0 then
            table.remove(self.sparks, index)
        end
    end

    if self.playback then
        if self.playback:hash() ~= self:hash() then
            self.mismatches = self.mismatches + 1
        end
        self.playback:advance()
    else
        self.recorder:advance(self:hash())
    end
end

function Replay:update(dt)
    Replay.super.update(self, dt)
    if self.statusTime >= Test.statusInterval then
        local mode = self.playback and string.format('Replaying step %d of %d, %d steps differ', self.playback.step, self.playback.steps, self.mismatches) or string.format('Recording step %d of at most %d', self.recorder.steps, kLimit)
        self:status(string.format('%s, the recording takes %d bytes', mode, #self.recorder:bytes()))
    end
end

function Replay:draw(area)
    local ground = area.height - 120
    local center = area.width * 0.5
    graphics2d.drawRect({24, ground, area.width - 48, 4}, Test.line)
    graphics2d.drawCircle(center + self.hopper.x, ground - 36 + self.hopper.y, 36, self.playback and Test.violet or Test.warm, {layer = 1})
    for _, spark in ipairs(self.sparks) do
        graphics2d.drawCircle(center + spark.x, ground - 36 + spark.y, 8 * spark.life, Test.accent, {layer = 2})
    end
    Test.caption(self.playback and 'Replay' or 'Recording', 24, 24, {size = 40, color = self.playback and Test.violet or Test.warm})
end

return Replay
