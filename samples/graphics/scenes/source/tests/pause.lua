-- Pause and process modes: haylen.setPaused stops the pausable world, its timers and its tweens, while the pause menu runs in whenPaused and one timer runs always.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Pause = haylen.class('Pause', sample.Test)

Pause.hints = 'Pause with the button, P or the Y button. The world, its spawner timer and its tween stop, the always timer keeps counting and the menu animates.'

-- The pause menu: transparent over the world, running only while the game is paused.
local PauseMenu = haylen.class('PauseMenu', scene.Scene)

function PauseMenu:init(world)
    self.world = world
    self.transparent = true
    self.processMode = 'whenPaused'
    self.glow = {alpha = 0.3}
end

function PauseMenu:enter()
    haylen.setPaused(true)
    tween.to(self.glow, 0.6, {alpha = 1}, {loop = 'yoyo', repeatCount = -1, ease = 'sine_in_out', owner = self})
    self.document = ui.mount(ui.column{
        justify = 'center',
        onCancel = function()
            self:close()
        end,
        ui.panel{
            width = 560,
            align = 'center',
            gap = 16,
            ui.label{text = 'Paused', font = 'title', textAlign = 'center', align = 'center'},
            ui.button{id = 'resume', text = 'Resume', variant = 'primary', align = 'stretch', onClick = function() self:close() end},
            ui.button{id = 'spawn', text = 'Spawn while paused', align = 'stretch', onClick = function() self.world:spawn() end},
        },
    }, {layer = 1, owner = self})
    self.document:command('resume', 'focus')
end

function PauseMenu:exit()
    haylen.setPaused(false)
end

function PauseMenu:close()
    if not scene.transitioning() and scene.top() == self then
        scene.pop()
    end
end

function PauseMenu:update(dt)
    self.world:describe()
    if input.pressed('pause') then
        self:close()
    end
end

function PauseMenu:renderUi()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect(area, {0.02, 0.03, 0.08, 0.35 + 0.25 * self.glow.alpha})
end

function Pause:init(entry)
    Pause.super.init(self, entry)
    self.dots = {}
    self.spins = 0
    self.always = 0
    self.platform = {x = 300}
end

function Pause:enter()
    Pause.super.enter(self)
    timer.every(0.4, function() self:spawn() end, {owner = self})
    timer.every(0.1, function() self.always = self.always + 0.1 end, {owner = self, processMode = 'always'})
    tween.to(self.platform, 1.5, {x = 1400}, {loop = 'yoyo', repeatCount = -1, ease = 'sine_in_out', owner = self})
end

function Pause:controls()
    return {ui.button{text = 'Pause', onClick = function() self:openMenu() end}}
end

function Pause:openMenu()
    if not scene.transitioning() and scene.top() == self then
        scene.push(PauseMenu(self))
    end
end

-- Drops a dot from the top. The spawner timer calls it while the game runs, and the menu can call it while paused.
function Pause:spawn()
    self.dots[#self.dots + 1] = {x = 200 + (#self.dots * 137) % 1100, y = 300, speed = 0}
    if #self.dots > 60 then
        table.remove(self.dots, 1)
    end
end

function Pause:describe()
    self:setStatus(string.format('paused %s, %d dots, world spins %d, always timer %.1f s', tostring(haylen.paused()), #self.dots, self.spins, self.always))
end

function Pause:update(dt)
    if input.pressed('pause') then
        self:openMenu()
    end
    self.spins = self.spins + 1
    for _, dot in ipairs(self.dots) do
        dot.speed = dot.speed + 900 * dt
        dot.y = math.min(900, dot.y + dot.speed * dt)
    end
    self:describe()
end

function Pause:render()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect(area, '#FF1E2A3A')
    graphics2d.drawRect({area.x, 930, area.width, 200}, '#FF2A3A2A')
    graphics2d.drawRect({self.platform.x - 120, 880, 240, 24}, '#FFFF9040')
    for _, dot in ipairs(self.dots) do
        graphics2d.drawCircle(dot.x, dot.y, 18, '#FF60C0FF')
    end
    graphics2d.drawText(nil, string.format('always %.1f', self.always), 1300, 520, {size = 48, color = '#FFE0E0E0'})
end

return Pause
