-- Pause: a pause menu in the `whenPaused` mode pauses the game, which stops the bouncing balls and the pausable timer of the test while a timer in the `always` mode keeps counting, and both scenes hear `paused` and `unpaused`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local EventsTest = require('categories.events.events-test')
local Journal = require('harness.journal')
local Overlay = require('categories.events.overlay')
local Test = require('harness.test')

local Pause = haylen.class('Pause', EventsTest)

local kCode = [[
PauseMenu.processMode = 'whenPaused'
function PauseMenu:enter() haylen.setPaused(true) end  function PauseMenu:exit() haylen.setPaused(false) end
timer.every(0.5, tick, {owner = level})  -- Inherits `pausable` from the level.
timer.every(0.5, tick, {owner = level, processMode = 'always'})  -- Counts through the pause.]]

Pause.pauseAction = {name = 'pause', type = 'button', bindings = {'key:p', 'button:start'}}

local PauseMenu = haylen.class('PauseMenu', Overlay)
PauseMenu.processMode = 'whenPaused'

function PauseMenu:enter(params)
    self.journal = params.journal
    self.seconds = 0
    haylen.setPaused(true)
    self:card('Paused', {ui.label{id = 'time', text = 'Paused for 0 seconds', color = 'textMuted'}})
    timer.every(1, function()
        self.seconds = self.seconds + 1
        self.gui:set('time', {text = string.format('Paused for %d %s', self.seconds, self.seconds == 1 and 'second' or 'seconds')})
    end, {owner = self})
end

function PauseMenu:exit()
    haylen.setPaused(false)
end

function PauseMenu:paused()
    self.journal:add('The pause menu got the "paused" hook', Test.muted)
end

function PauseMenu:unpaused()
    self.journal:add('The pause menu got the "unpaused" hook, so it runs now', Test.green)
end

function PauseMenu:update(dt)
    if input.pressed('pause') then
        self:close()
    end
end

function Pause:enter()
    self.journal = Journal()
    self.ticks = {pausable = 0, always = 0}
    self.balls = {}
    timer.every(0.5, function() self.ticks.pausable = self.ticks.pausable + 1 end, {owner = self})
    timer.every(0.5, function() self.ticks.always = self.ticks.always + 1 end, {owner = self, processMode = 'always'})
    self:listen('paused', function() self.journal:add('Event "paused"', Test.warm) end)
    self:listen('unpaused', function() self.journal:add('Event "unpaused"', Test.warm) end)
    self:frame({
        hint = 'Open the pause menu with the button, P or the start button, and close it the same way.',
        code = kCode,
        controls = {ui.button{id = 'open', text = 'Open the pause menu', variant = 'primary', onClick = function() self:openMenu() end}},
        focus = 'open',
    })
    self:loadActions({actions = {Pause.pauseAction}})
end

-- The balls bounce in the upper half of the stage, so they start once the stage has a size.
function Pause:resize(area)
    if #self.balls == 0 then
        for index = 1, 8 do
            self.balls[index] = {x = index * area.width / 9, y = area.height / 4, vx = 180 + index * 25, vy = 140 - index * 20}
        end
    end
end

function Pause:openMenu()
    PauseMenu():open({journal = self.journal})
end

function Pause:paused()
    self.journal:add('The test got the "paused" hook, so it stops', Test.red)
end

function Pause:unpaused()
    self.journal:add('The test got the "unpaused" hook and runs again', Test.green)
end

function Pause:update(dt)
    Pause.super.update(self, dt)
    if input.pressed('pause') then
        self:openMenu()
    end
    for _, ball in ipairs(self.balls) do
        ball.x, ball.y = ball.x + ball.vx * dt, ball.y + ball.vy * dt
        if ball.x < 30 or ball.x > self.area.width - 30 then
            ball.vx = -ball.vx
        end
        if ball.y < 30 or ball.y > self.area.height / 2 - 30 then
            ball.vy = -ball.vy
        end
    end
    self:status(string.format('Game %s, ticks of the "pausable" timer %d, ticks of the "always" timer %d', haylen.paused() and 'paused' or 'running', self.ticks.pausable, self.ticks.always))
end

function Pause:draw(area)
    graphics2d.drawRectOutline({0, 0, area.width, area.height / 2}, 2, Test.line)
    for _, ball in ipairs(self.balls) do
        graphics2d.drawCircle(ball.x, ball.y, 26, haylen.paused() and Test.muted or Test.accent)
    end
    graphics2d.drawText(nil, string.format('Timer "pausable" %d, timer "always" %d', self.ticks.pausable, self.ticks.always), area.width - 30, 20, {size = 30, color = Test.warm, anchor = {1, 0}})
    self.journal:draw(24, area.height / 2 + 16, area.height / 2 - 32, 26)
end

return Pause
