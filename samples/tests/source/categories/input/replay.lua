-- Replay: a hopper that moves and jumps on fixed steps while a recording keeps its buttons and the state of the generator of its sparks, then plays the run back in place of the devices, with the hash of each step that shows the replay matches the run.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Backdrop = require('categories.animation.backdrop')
local Test = require('harness.test')
local prompts = require('categories.input.prompts')

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
-- The arrows of the touch buttons, drawn from vector paths.
local kArrowLeft = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M40 12 L18 32 L40 52 Z" fill="#FFFFFF" stroke="#15172A" stroke-width="4" stroke-linejoin="round"/></svg>'
local kArrowRight = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M24 12 L46 32 L24 52 Z" fill="#FFFFFF" stroke="#15172A" stroke-width="4" stroke-linejoin="round"/></svg>'
-- The walker sheet has cells of 256 pixels in eight columns: the idle frames in the first row and the run in the second, with a frame of the run with the legs apart for the air.
local kIdleFrames = {0, 1, 2, 3}
local kRunFrames = {8, 9, 10, 11, 12, 13, 14, 15}
local kAir = 10
local kFeet = 248 / 256
local kHeroSize = 150

function Replay:enter()
    self:loadActions(kActions)
    ui.setImage('@replayLeft', graphics.newVectorImage(kArrowLeft))
    ui.setImage('@replayRight', graphics.newVectorImage(kArrowRight))
    self.walker = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.backdrop = Backdrop()
    self.time = 0
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
                ui.touchButton{action = 'left', image = '@replayLeft', size = 100, touchOnly = true},
                ui.touchButton{action = 'right', image = '@replayRight', size = 100, touchOnly = true},
                ui.touchButton{action = 'jump', text = 'Jump', size = 100, touchOnly = true},
            },
        },
    })
end

function Replay:exit()
    Replay.super.exit(self)
    input.clearVirtual()
    ui.removeImage('@replayLeft')
    ui.removeImage('@replayRight')
end

function Replay:resetWorld(randomState)
    self.hopper = {x = 0, y = 0, vy = 0, facing = 1, moving = false}
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
    hopper.moving = direction ~= 0
    if direction ~= 0 then
        hopper.facing = direction
    end
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
    self.time = self.time + dt
    if self.statusTime >= Test.statusInterval then
        local mode = self.playback and string.format('Replaying step %d of %d, and %d steps differ from the run', self.playback.step, self.playback.steps, self.mismatches) or string.format('Recording step %d of at most %d', self.recorder.steps, kLimit)
        self:status(string.format('%s. The recording takes %d bytes.', mode, #self.recorder:bytes()))
    end
end

-- Draws the mode in the corner: a red dot that pulses while the run records, or a green triangle while it plays back, and the hash of the current step under it.
function Replay:drawMode(x, y)
    local replaying = self.playback ~= nil
    local text = replaying and 'Replay' or 'Recording'
    local width = graphics2d.measureText(prompts.font(), text, {size = 30}) + 84
    graphics2d.drawShape({x, y, width, 56}, {radius = 28, color = '#E6151A2A', borderWidth = 2, borderColor = replaying and Test.green or Test.red, layer = 5})
    if replaying then
        graphics2d.drawPolygon({{x + 24, y + 16}, {x + 46, y + 28}, {x + 24, y + 40}}, Test.green, {layer = 6})
    else
        local pulse = 0.55 + 0.45 * math.sin(self.time * 6)
        graphics2d.drawShape({x + 20, y + 16, 24, 24}, {kind = 'ellipse', color = {1, 0.3, 0.27, pulse}, layer = 6})
    end
    graphics2d.drawText(prompts.font(), text, x + 60, y + 28, {size = 30, anchor = {0, 0.5}, outlineWidth = 3, outlineColor = prompts.ink, layer = 6})
    local hash = string.format('Step hash %08X', self:hash())
    local hashWidth = graphics2d.measureText(nil, hash, {size = 22}) + 40
    graphics2d.drawShape({x, y + 68, hashWidth, 40}, {radius = 20, color = '#E6151A2A', layer = 5})
    graphics2d.drawText(nil, hash, x + 20, y + 88, {size = 22, color = replaying and Test.green or Test.ink, anchor = {0, 0.5}, layer = 6})
end

function Replay:draw(area)
    local ground = area.height - 60
    local center = area.width * 0.5
    self.backdrop:draw(graphics2d.canvasBounds(), ground, 150, 0)
    local hopper = self.hopper
    local frame = hopper.y < 0 and kAir or (hopper.moving and kRunFrames[math.floor(self.time * 14) % #kRunFrames + 1] or kIdleFrames[math.floor(self.time * 6) % #kIdleFrames + 1])
    Backdrop.shadow(center + hopper.x, ground, hopper.y < 0 and 60 or 90, 3)
    graphics2d.draw(self.walker, center + hopper.x, ground + hopper.y, {source = {frame % 8 * 256, frame // 8 * 256, 256, 256}, width = kHeroSize, height = kHeroSize, pivotY = kFeet, flipHorizontal = hopper.facing < 0, layer = 4})
    for _, spark in ipairs(self.sparks) do
        local x, y = center + spark.x, ground - 20 + spark.y
        local size = 34 * spark.life
        graphics2d.drawShape({x - size, y - size, size * 2, size * 2}, {kind = 'ellipse', color = {1, 0.85, 0.35, 0.5 * spark.life}, softness = size, blend = 'additive', layer = 5})
        graphics2d.drawCircle(x, y, 6 * spark.life, '#FFFFF6D8', {layer = 5})
    end
    self:drawMode(24, 24)
end

return Replay
