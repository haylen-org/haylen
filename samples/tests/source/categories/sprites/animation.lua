-- Animation: a walker with idle, run and attack clips driven by buttons and a queue, its frame and finish events in a list, and the run clip on three walkers that loop, play once and ping-pong, all on a meadow.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Landscape = require('categories.sprites.landscape')
local Plate = require('categories.sprites.plate')
local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local Animation = haylen.class('Animation', SpriteTest)

Animation.cells = {idle = {1, 2, 3, 4}, run = {9, 10, 11, 12, 13, 14, 15, 16}, attack = {17, 18, 19, 20, 21, 22}}
Animation.loops = {{mode = 'loop', label = 'Loops'}, {mode = 'once', label = 'Plays once'}, {mode = 'pingPong', label = 'Ping-pongs'}}
Animation.lines = 7
Animation.groundHeight = 76
-- The feet of the walker stand on this share of the height of its cells.
Animation.feet = 0.97

Animation.code = [[
local grid = {frameWidth = 256, frameHeight = 256, cells = {17, 18, 19, 20, 21, 22}, loop = 'once'}
animator:add('attack', animation2d.fromGrid(sheet, grid))
animator.onFrame = function(name, frame) if frame == 3 then log.info('The blow lands.') end end
animator.onComplete = function(name) log.info('The clip "' .. name .. '" finished.') end
animator:play('attack', true)
animator:queue('run')]]

-- Adds the clips of the walker sheet to an animator, with the run clip in the given loop mode.
function Animation.clips(animator, sheet, runLoop)
    local cells = Animation.cells
    animator:add('idle', animation2d.fromGrid(sheet, {frameWidth = 256, frameHeight = 256, cells = cells.idle, framesPerSecond = 6}))
    animator:add('run', animation2d.fromGrid(sheet, {frameWidth = 256, frameHeight = 256, cells = cells.run, framesPerSecond = 12, loop = runLoop}))
    animator:add('attack', animation2d.fromGrid(sheet, {frameWidth = 256, frameHeight = 256, cells = cells.attack, framesPerSecond = 12, loop = 'once'}))
    animator.pivotY = Animation.feet
end

function Animation:enter()
    local sheet = SpriteTest.texture('sheets/walker.png')
    self.landscape = Landscape()
    self.time = 0
    self.history = {}
    self.hero = {sprite = graphics2d.newSprite(sheet, {scaleX = 1.15, scaleY = 1.15, layer = 2}), animator = animation2d.newAnimator()}
    Animation.clips(self.hero.animator, sheet, 'loop')
    self.hero.animator:play('idle')
    self.hero.animator.onFrame = function(name, frame)
        if name == 'attack' and frame == 3 then
            self:record('Frame 3 of "attack" lands the blow')
        end
    end
    self.hero.animator.onComplete = function(name) self:record('The clip "' .. name .. '" finished') end

    self.walkers = {}
    for index, loop in ipairs(Animation.loops) do
        local entry = {sprite = graphics2d.newSprite(sheet, {scaleX = 0.62, scaleY = 0.62, layer = 2}), animator = animation2d.newAnimator()}
        Animation.clips(entry.animator, sheet, loop.mode)
        entry.animator:play('run')
        self.walkers[index] = entry
    end

    self:frame{
        code = Animation.code,
        hint = 'Click or tap the meadow to attack. A queued clip starts when the clip before it ends its pass.',
        controls = {
            ui.row{gap = 12,
                ui.button{id = 'idle', text = 'Idle', grow = 1, onClick = function() self:play('idle') end},
                ui.button{id = 'run', text = 'Run', grow = 1, onClick = function() self:play('run') end},
                ui.button{id = 'attack', text = 'Attack', grow = 1, variant = 'primary', onClick = function() self:play('attack') end},
            },
            ui.button{id = 'queue', text = 'Attack, then run, then idle', align = 'stretch', onClick = function() self:combo() end},
            ui.button{id = 'stop', text = 'Stop', align = 'stretch', onClick = function()
                self.hero.animator:stop()
                self:record('Stopped')
            end},
            ui.formField{label = 'Speed', ui.slider{id = 'speed', min = 0.25, max = 3, value = 1, step = 0.25, showValue = true, onChange = function(event) self.hero.animator.speed = event.value end}},
            ui.button{id = 'restart', text = 'Restart the three runners', align = 'stretch', onClick = function()
                for _, entry in ipairs(self.walkers) do
                    entry.animator:play('run', true)
                end
            end},
        },
        focus = 'attack',
    }
end

-- Adds a line to the list on the stage and to the log.
function Animation:record(text)
    table.insert(self.history, text)
    if #self.history > Animation.lines then
        table.remove(self.history, 1)
    end
    self:log('%s.', text)
end

function Animation:play(name)
    self.hero.animator:play(name, true)
    self:record('Play "' .. name .. '"')
end

function Animation:combo()
    self:play('attack')
    self.hero.animator:queue('run')
    self.hero.animator:queue('idle')
    self:record('Queued "run" and "idle"')
end

function Animation.advance(entry, dt)
    entry.animator:update(dt)
    entry.animator:apply(entry.sprite)
end

function Animation:resize(area)
    self.ground = area.height - Animation.groundHeight
end

function Animation:update(dt)
    Animation.super.update(self, dt)
    self.time = self.time + dt
    local _, _, pressed = self:pointer()
    if pressed then
        self:play('attack')
    end
    Animation.advance(self.hero, dt)
    for _, entry in ipairs(self.walkers) do
        Animation.advance(entry, dt)
    end

    local animator = self.hero.animator
    local frames = #Animation.cells[animator.current]
    local state = 'plays'
    if animator.finished then
        state = 'finished'
    elseif not animator.playing then
        state = 'stopped on'
    end
    local queued = animator.queuedCount == 0 and 'nothing queued' or (animator.queuedCount == 1 and 'one clip queued' or animator.queuedCount .. ' clips queued')
    self:status(string.format('The hero %s "%s" at frame %d of %d, at %.2f times the speed, with %s.', state, animator.current, animator.frame, frames, animator.speed, queued))
end

-- Draws the events of the hero on a card in the sky, newest last, with a line that says what to do before the first one.
function Animation:drawHistory(area)
    local width = math.min(620, area.width * 0.46)
    local card = {area.width - width - 32, 32, width, 76 + math.max(1, #self.history) * 36}
    graphics2d.drawShape(card, {radius = 16, color = '#D81A1D2C', borderWidth = 2, borderColor = '#FF3A4160', layer = 5})
    graphics2d.drawText(nil, 'Events of the hero', card[1] + 24, card[2] + 20, {size = 26, color = Test.warm, layer = 6})
    if #self.history == 0 then
        graphics2d.drawText(nil, 'Click or tap the meadow to attack.', card[1] + 24, card[2] + 66, {size = 24, color = Plate.ink, layer = 6})
        return
    end
    for index, line in ipairs(self.history) do
        graphics2d.drawText(nil, line, card[1] + 24, card[2] + 30 + index * 36, {size = 24, color = index == #self.history and Plate.ink or '#FF9AA2BE', layer = 6})
    end
end

function Animation:draw(area)
    self.landscape:draw(area, self.ground, 0.5, self.time)
    local hero = self.hero.sprite
    hero.x, hero.y = area.width * 0.2, self.ground
    graphics2d.drawEllipse(hero.x, self.ground - 4, 70, 14, '#40000000', {layer = 1})
    hero:draw()
    self:drawHistory(area)

    local left, right = area.width * 0.44, area.width - 120
    for index, entry in ipairs(self.walkers) do
        local x = left + (right - left) * (index - 1) / (#self.walkers - 1)
        entry.sprite.x, entry.sprite.y = x, self.ground
        graphics2d.drawEllipse(x, self.ground - 4, 40, 9, '#40000000', {layer = 1})
        entry.sprite:draw()
        Plate.draw(Animation.loops[index].label, x, self.ground + Animation.groundHeight * 0.5)
    end
end

return Animation
