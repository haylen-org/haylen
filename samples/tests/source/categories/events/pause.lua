-- Pause: a pause menu in the `whenPaused` mode pauses the game, which stops the coins that bounce over a meadow and the pausable timer of the test while a timer in the `always` mode keeps counting, and both scenes hear `paused` and `unpaused`.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
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
Pause.coinRadius = 32
-- The ground strip of the meadow is 1076 by 256 pixels with the grass this share of its height below its top, and the hills are 1096 by 538 pixels.
Pause.groundSize = {1076, 256}
Pause.footing = 70 / 256
Pause.hillsSize = {1096, 538}
Pause.stripHeight = 110

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
    self.hills = assets.texture('camera/hills.png', {filter = 'linear'})
    self.ground = assets.texture('camera/ground.png', {filter = 'linear'})
    local sky = graphics.newImage(4, 256)
    sky:fillGradient({angle = math.pi / 2, stops = {{0, '#FF5FB4EC'}, {0.75, '#FFAEDDF6'}, {1, '#FFE4F5FB'}}})
    self.sky = graphics.newTexture(sky, {filter = 'linear'})
    self.worldShape = function() graphics2d.drawShape(self.world, {radius = 18}) end
    timer.every(0.5, function() self.ticks.pausable = self.ticks.pausable + 1 end, {owner = self})
    timer.every(0.5, function() self.ticks.always = self.ticks.always + 1 end, {owner = self, processMode = 'always'})
    self:listen('paused', function() self.journal:add('The event "paused" reaches its listeners.', Test.warm) end)
    self:listen('unpaused', function() self.journal:add('The event "unpaused" reaches its listeners.', Test.warm) end)
    self:frame({
        hint = 'Open the pause menu with the button, P or the start button, and close it the same way.',
        code = kCode,
        controls = {
            ui.button{id = 'open', text = 'Open the pause menu', variant = 'primary', onClick = function() self:openMenu() end},
            ui.label{text = 'Process modes', font = 'caption', color = 'textMuted'},
            ui.label{text = 'The mode "pausable" stops while the game is paused, like the level, its coins and its first timer.'},
            ui.label{text = 'The mode "always" runs paused or not, like the second timer.'},
            ui.label{text = 'The mode "whenPaused" runs only while the game is paused, like the pause menu.'},
            ui.label{text = 'The mode "inherit" takes the mode of the owner, which gives the first timer the mode of the level.'},
            ui.label{text = 'The mode "disabled" never runs.'},
        },
        focus = 'open',
    })
    self:loadActions({actions = {Pause.pauseAction}})
end

-- The coins bounce over the meadow of the world box, between its top and the grass, so they start once the stage has a size.
function Pause:resize(area)
    local world = m.rect(24, EventsTest.titleRoom, area.width - 48, math.max(240, area.height * 0.56 - EventsTest.titleRoom + 24))
    self.world, self.grass = world, world:bottom() - 44
    self.bounds = {world.x, world.y, world.width, self.grass - world.y}
    if #self.coins == 0 then
        for index = 1, 8 do
            self.coins[index] = {x = world.x + index * world.width / 9, y = world.y + self.bounds[4] / 2, vx = 180 + index * 25, vy = 140 - index * 20, spin = index * 0.7}
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
    local bounds, radius = self.bounds, Pause.coinRadius
    for _, coin in ipairs(self.coins) do
        coin.x, coin.y, coin.spin = coin.x + coin.vx * dt, coin.y + coin.vy * dt, coin.spin + dt * 4
        if coin.x < bounds[1] + radius or coin.x > bounds[1] + bounds[3] - radius then
            coin.vx = -coin.vx
            coin.x = math.max(bounds[1] + radius, math.min(bounds[1] + bounds[3] - radius, coin.x))
        end
        if coin.y < bounds[2] + radius or coin.y > bounds[2] + bounds[4] - radius then
            coin.vy = -coin.vy
            coin.y = math.max(bounds[2] + radius, math.min(bounds[2] + bounds[4] - radius, coin.y))
        end
    end
    self:collideCoins()
end

-- Coins that touch push apart and trade the speed along the line between them, as balls of the same weight do, so no two coins overlap.
function Pause:collideCoins()
    local coins, reach = self.coins, Pause.coinRadius * 2
    for first = 1, #coins - 1 do
        for second = first + 1, #coins do
            local a, b = coins[first], coins[second]
            local dx, dy = b.x - a.x, b.y - a.y
            local distance = math.sqrt(dx * dx + dy * dy)
            if distance < reach and distance > 0 then
                local nx, ny = dx / distance, dy / distance
                local push = (reach - distance) / 2
                a.x, a.y, b.x, b.y = a.x - nx * push, a.y - ny * push, b.x + nx * push, b.y + ny * push
                local closing = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny
                if closing < 0 then
                    a.vx, a.vy, b.vx, b.vy = a.vx + closing * nx, a.vy + closing * ny, b.vx - closing * nx, b.vy - closing * ny
                end
            end
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

-- Draws the meadow of the level in the world box: a sky that pales toward the hills, the hills and the ground strip with its grass on the line the coins bounce on.
function Pause:drawMeadow(world)
    local strip = Pause.stripHeight
    local top = self.grass - strip * Pause.footing
    graphics2d.pushMask(self.worldShape)
    graphics2d.draw(self.sky, world.x, world.y, {width = world.width, height = top - world.y + strip * 0.5, pivotX = 0, pivotY = 0, layer = 0})
    local hillsHeight = strip * 1.5
    local hillsWidth = hillsHeight * Pause.hillsSize[1] / Pause.hillsSize[2]
    for x = world.x, world:right(), hillsWidth do
        graphics2d.draw(self.hills, x, top + strip * 0.4, {width = hillsWidth + 1, height = hillsHeight, pivotX = 0, pivotY = 1, layer = 0})
    end
    local stripWidth = strip * Pause.groundSize[1] / Pause.groundSize[2]
    for x = world.x, world:right(), stripWidth do
        graphics2d.draw(self.ground, x, top, {width = stripWidth + 1, height = strip, pivotX = 0, pivotY = 0, layer = 1})
    end
    graphics2d.popMask()
    graphics2d.drawShape(world, {radius = 18, color = '#00000000', borderWidth = 2, borderColor = EventsTest.cardBorder, layer = 8})
end

function Pause:draw(area)
    local world = self.world
    local paused = haylen.paused()
    self:label('The level under the pause menu', 24, 48, {anchor = {0, 0.5}, size = 26})
    self:drawMeadow(world)
    for _, coin in ipairs(self.coins) do
        local size = Pause.coinRadius * 2
        graphics2d.draw(self.coin, coin.x, coin.y, {width = size * math.max(0.15, math.abs(math.cos(coin.spin))), height = size, layer = 2})
    end
    if paused then
        graphics2d.drawShape(world, {radius = 18, color = '#9910121C', layer = 5})
        self:label('Paused', world.x + world.width / 2, world.y + world.height / 2 - 20, {size = 64, layer = 7})
    end
    local left, top = world.x + 20, world.y + 20
    local width = self:drawTimer('pausable', self.ticks.pausable, left, top, not paused)
    self:drawTimer('always', self.ticks.always, left + width + 16, top, true)
    local logTop = world:bottom() + 16
    self:logPanel(self.journal, {24, logTop, area.width - 48, area.height - logTop - 24}, 'What happened', {'bell', 'Open the pause menu to see who hears the pause, and in what order.'})
end

return Pause
