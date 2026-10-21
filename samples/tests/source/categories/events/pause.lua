-- Pause: a pause menu in the `whenPaused` mode pauses the game, which stops the bouncing coins and the pausable timer of the test while a timer in the `always` mode keeps counting, and both scenes hear `paused` and `unpaused`.
local assets = require('haylen.assets')
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
function PauseMenu:enter() haylen.setPaused(true) end
function PauseMenu:exit() haylen.setPaused(false) end
timer.every(0.5, tick, {owner = level})  -- Inherits "pausable" from the level.
timer.every(0.5, tick, {owner = level, processMode = 'always'})  -- Counts through the pause.
function level:paused() print('The level stops.') end]]

Pause.pauseAction = {name = 'pause', type = 'button', bindings = {'key:p', 'button:start'}}
Pause.coinRadius = 40

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
    self.journal:add('The pause menu got the "paused" hook.', Test.muted)
end

function PauseMenu:unpaused()
    self.journal:add('The pause menu got the "unpaused" hook, so it runs now.', Test.green)
end

function PauseMenu:update(dt)
    if input.pressed('pause') then
        self:close()
    end
end

function Pause:enter()
    self.journal = Journal()
    self.ticks = {pausable = 0, always = 0}
    self.coins = {}
    self.coin = assets.texture('physics/coin.png', {filter = 'linear'})
    timer.every(0.5, function() self.ticks.pausable = self.ticks.pausable + 1 end, {owner = self})
    timer.every(0.5, function() self.ticks.always = self.ticks.always + 1 end, {owner = self, processMode = 'always'})
    self:listen('paused', function() self.journal:add('The event "paused" reaches its listeners.', Test.warm) end)
    self:listen('unpaused', function() self.journal:add('The event "unpaused" reaches its listeners.', Test.warm) end)
    self:frame({
        hint = 'Open the pause menu with the button, P or the start button, and close it the same way.',
        code = kCode,
        controls = {ui.button{id = 'open', text = 'Open the pause menu', variant = 'primary', onClick = function() self:openMenu() end}},
        focus = 'open',
    })
    self:loadActions({actions = {Pause.pauseAction}})
end

-- The coins bounce inside the world box, so they start once the stage has a size.
function Pause:resize(area)
    self.world = {24, EventsTest.counterRoom, area.width - 48, math.max(200, area.height * 0.5 - EventsTest.counterRoom + 24)}
    if #self.coins == 0 then
        local world = self.world
        for index = 1, 8 do
            self.coins[index] = {x = world[1] + index * world[3] / 9, y = world[2] + world[4] / 2, vx = 180 + index * 25, vy = 140 - index * 20, spin = index * 0.7}
        end
    end
end

function Pause:openMenu()
    PauseMenu():open({journal = self.journal})
end

function Pause:paused()
    self.journal:add('The level got the "paused" hook, so it stops.', Test.red)
end

function Pause:unpaused()
    self.journal:add('The level got the "unpaused" hook and runs again.', Test.green)
end

function Pause:update(dt)
    Pause.super.update(self, dt)
    if input.pressed('pause') then
        self:openMenu()
    end
    local world, radius = self.world, Pause.coinRadius
    for _, coin in ipairs(self.coins) do
        coin.x, coin.y, coin.spin = coin.x + coin.vx * dt, coin.y + coin.vy * dt, coin.spin + dt * 4
        if coin.x < world[1] + radius or coin.x > world[1] + world[3] - radius then
            coin.vx = -coin.vx
            coin.x = math.max(world[1] + radius, math.min(world[1] + world[3] - radius, coin.x))
        end
        if coin.y < world[2] + radius or coin.y > world[2] + world[4] - radius then
            coin.vy = -coin.vy
            coin.y = math.max(world[2] + radius, math.min(world[2] + world[4] - radius, coin.y))
        end
    end
end

-- The test does not update while the pause menu is on top of it, so the status line changes as it renders.
function Pause:renderUi()
    local text = haylen.paused() and 'The game is paused: the coins and the "pausable" timer stop while the "always" timer counts on.' or 'The game runs, so both timers count and the coins bounce.'
    if text ~= self.shown then
        self.shown = text
        Test.setNode(self, 'status', {text = text})
    end
end

-- Draws a timer as a pill with its name and its count, at the bottom of the world box, and returns its width.
function Pause:drawTimer(name, count, x, y, running)
    local text = string.format('Timer "%s": %d %s', name, count, count == 1 and 'tick' or 'ticks')
    local width = graphics2d.measureText(self.font, text, {size = 24}) + 72
    graphics2d.drawShape({x, y, width, 56}, {radius = 28, color = '#E6151A2A', borderWidth = 2, borderColor = running and Test.green or Test.muted, layer = 6})
    graphics2d.drawShape({x + 16, y + 18, 20, 20}, {kind = 'ellipse', color = running and Test.green or Test.muted, layer = 7})
    self:label(text, x + 48, y + 28, {anchor = {0, 0.5}, size = 24, layer = 7})
    return width
end

function Pause:draw(area)
    local world = self.world
    local paused = haylen.paused()
    self:label('The level under the pause menu', 24, 48, {anchor = {0, 0.5}, size = 26})
    graphics2d.drawShape(world, {radius = 18, color = '#FF2B4A6E', borderWidth = 2, borderColor = EventsTest.cardBorder})
    graphics2d.drawShape({world[1], world[2] + world[4] * 0.55, world[3], world[4] * 0.45}, {radius = {0, 0, 18, 18}, color = '#FF345A82', layer = 0})
    for _, coin in ipairs(self.coins) do
        local size = Pause.coinRadius * 2
        graphics2d.draw(self.coin, coin.x, coin.y, {width = size * math.max(0.15, math.abs(math.cos(coin.spin))), height = size, layer = 2})
    end
    if paused then
        graphics2d.drawShape(world, {radius = 18, color = '#9910121C', layer = 5})
        self:label('Paused', world[1] + world[3] / 2, world[2] + world[4] / 2 - 20, {size = 64, layer = 7})
    end
    local left = world[1] + 20
    local top = world[2] + world[4] - 72
    local width = self:drawTimer('pausable', self.ticks.pausable, left, top, not paused)
    self:drawTimer('always', self.ticks.always, left + width + 16, top, true)
    local logTop = world[2] + world[4] + 16
    self:logPanel(self.journal, {24, logTop, area.width - 48, area.height - logTop - 24}, 'What happened', {'bell', 'Open the pause menu to see who hears the pause, and in what order.'})
end

return Pause
