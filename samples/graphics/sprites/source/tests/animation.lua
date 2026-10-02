-- Animation: a walker with idle, run and attack clips driven by buttons and a queue, its frame and finish events in a log, and the run clip on three walkers that loop, play once and ping-pong.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local Animation = haylen.class('Animation', sample.Test)

local kLoops = {'loop', 'once', 'pingPong'}
local kLogLines = 8
local kCode = [[
animator:add('attack', animation2d.fromGrid(sheet, {frameWidth = 64, frameHeight = 64, cells = {17, 18, 19, 20, 21, 22}, framesPerSecond = 12, loop = 'once'}))
animator.onFrame = function(name, frame) if name == 'attack' and frame == 3 then print('hit') end end
animator.onFinish = function(name) print(name .. ' finished') end
animator:play('attack', true)  animator:queue('run')  animator:queue('idle')  animator.speed = 1.5  animator:update(dt)  animator:apply(sprite)]]

-- Adds the clips of the walker sheet to an animator, with the run clip in the given loop mode.
local function clips(animator, sheet, runLoop)
    animator:add('idle', animation2d.fromGrid(sheet, {frameWidth = 64, frameHeight = 64, cells = {1, 2, 3, 4}, framesPerSecond = 6}))
    animator:add('run', animation2d.fromGrid(sheet, {frameWidth = 64, frameHeight = 64, cells = {9, 10, 11, 12, 13, 14, 15, 16}, framesPerSecond = 12, loop = runLoop}))
    animator:add('attack', animation2d.fromGrid(sheet, {frameWidth = 64, frameHeight = 64, cells = {17, 18, 19, 20, 21, 22}, framesPerSecond = 12, loop = 'once'}))
end

function Animation:enter()
    local sheet = sample.texture('sheets/walker.png')
    self.lines = {}
    self.hero = {sprite = graphics2d.newSprite(sheet, {scaleX = 4, scaleY = 4}), animator = animation2d.newAnimator()}
    clips(self.hero.animator, sheet, 'loop')
    self.hero.animator.pivotY = 0.9
    self.hero.animator:play('idle')
    self.hero.animator.onFrame = function(name, frame)
        if name == 'attack' and frame == 3 then
            self:log('Callback "onFrame" at frame 3 of "attack": the blow lands')
        end
    end
    self.hero.animator.onFinish = function(name) self:log('Callback "onFinish" for "' .. name .. '"') end

    self.loops = {}
    for index, mode in ipairs(kLoops) do
        local entry = {sprite = graphics2d.newSprite(sheet, {scaleX = 2, scaleY = 2}), animator = animation2d.newAnimator()}
        clips(entry.animator, sheet, mode)
        entry.animator:play('run')
        self.loops[index] = entry
    end

    self:frame({
        hint = 'Click or tap the stage to attack. Queued clips start when the current pass ends.',
        code = kCode,
        controls = {
            ui.row{gap = 12,
                ui.button{id = 'idle', text = 'Idle', grow = 1, onClick = function() self:play('idle') end},
                ui.button{id = 'run', text = 'Run', grow = 1, onClick = function() self:play('run') end},
                ui.button{id = 'attack', text = 'Attack', grow = 1, variant = 'primary', onClick = function() self:play('attack') end},
            },
            ui.button{id = 'queue', text = 'Attack, then run, then idle', onClick = function() self:combo() end},
            ui.button{id = 'stop', text = 'Stop', onClick = function() self.hero.animator:stop() end},
            ui.formField{label = 'Speed', ui.slider{id = 'speed', min = 0.25, max = 3, value = 1, step = 0.25, showValue = true, onChange = function(event) self.hero.animator.speed = event.value end}},
            ui.button{id = 'restart', text = 'Restart the loop modes', onClick = function()
                for _, entry in ipairs(self.loops) do
                    entry.animator:play('run', true)
                end
            end},
        },
        focus = 'attack',
    })
end

function Animation:log(text)
    table.insert(self.lines, text)
    if #self.lines > kLogLines then
        table.remove(self.lines, 1)
    end
end

function Animation:play(name)
    self.hero.animator:play(name, true)
    self:log('Play "' .. name .. '"')
end

function Animation:combo()
    self:play('attack')
    self.hero.animator:queue('run')
    self.hero.animator:queue('idle')
    self:log('Queued "run" and "idle", ' .. self.hero.animator.queuedCount .. ' waiting')
end

function Animation:update(dt)
    Animation.super.update(self, dt)
    local _, _, pressed = self:pointer()
    if pressed then
        self:play('attack')
    end
    for _, entry in ipairs({self.hero, table.unpack(self.loops)}) do
        entry.animator:update(dt)
        entry.animator:apply(entry.sprite)
    end
    local animator = self.hero.animator
    self:status(string.format('Clip %s   frame %d   time %.2f   playing %s   finished %s   queued %d   speed %.2f', animator.current, animator.frame, animator.time, animator.playing, animator.finished, animator.queuedCount, animator.speed))
end

function Animation:draw(area)
    local hero = self.hero.sprite
    hero.x, hero.y = area.width * 0.25, area.height * 0.55
    hero:draw()
    for index, line in ipairs(self.lines) do
        graphics2d.drawText(nil, line, area.width * 0.5, 30 + (index - 1) * 34, {size = 26, color = index == #self.lines and sample.warm or sample.muted})
    end
    for index, entry in ipairs(self.loops) do
        local x = area.width * (0.45 + index * 0.13)
        entry.sprite.x, entry.sprite.y = x, area.height * 0.8
        entry.sprite:draw()
        graphics2d.drawText(nil, 'Loop "' .. kLoops[index] .. '"', x, area.height * 0.8 + 80, {size = 24, color = sample.ink, anchor = {0.5, 0.5}})
    end
end

return Animation
