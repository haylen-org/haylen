-- Pause and process modes: `haylen.setPaused` stops the pausable world, its timers and its tweens, while the pause menu runs in `whenPaused` and one timer runs `always`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local SceneTest = require('categories.scenes.scene-test')
local Test = require('harness.test')

local Pause = haylen.class('Pause', SceneTest)

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
    tween.to(self.glow, 0.6, {alpha = 1}, {loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut', owner = self})
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
    graphics2d.drawRect(graphics2d.canvasBounds(), {0.02, 0.03, 0.08, 0.35 + 0.25 * self.glow.alpha})
end

function Pause:init(entry)
    Pause.super.init(self, entry)
    self.dots = {}
    self.spins = 0
    self.always = 0
    self.platform = {x = 0.2}
end

function Pause:enter()
    self:frame{
        hint = 'Pause with the button, P or the Y button. The world, its spawner timer and its tween stop, the "always" timer keeps counting and the menu animates.',
        play = true,
        controls = {ui.button{id = 'pause', text = 'Pause', onClick = function() self:openMenu() end}},
    }
    timer.every(0.4, function() self:spawn() end, {owner = self})
    timer.every(0.1, function() self.always = self.always + 0.1 end, {owner = self, processMode = 'always'})
    tween.to(self.platform, 1.5, {x = 0.8}, {loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut', owner = self})
end

function Pause:openMenu()
    if not scene.transitioning() and scene.top() == self then
        scene.push(PauseMenu(self))
    end
end

-- Drops a dot from the top. The spawner timer calls it while the game runs, and the menu can call it while paused.
function Pause:spawn()
    self.dots[#self.dots + 1] = {x = 0.1 + (#self.dots * 0.137) % 0.8, y = 0, speed = 0}
    if #self.dots > 60 then
        table.remove(self.dots, 1)
    end
end

-- Shows the state in the status line, from the update of this test while it runs and from the update of the menu while the game is paused.
function Pause:describe()
    local text = string.format('Paused "%s"   Dots %d   World updates %d   Timer "always" %.1f s', haylen.paused(), #self.dots, self.spins, self.always)
    if text ~= self.shown then
        self.shown = text
        self:set('status', {text = text})
    end
end

function Pause:update(dt)
    Pause.super.update(self, dt)
    if input.pressed('pause') then
        self:openMenu()
    end
    self.spins = self.spins + 1
    local floor = self.area and self.area.height - 120 or 0
    for _, dot in ipairs(self.dots) do
        dot.speed = dot.speed + 900 * dt
        dot.y = math.min(floor, dot.y + dot.speed * dt)
    end
    self:describe()
end

function Pause:draw(area)
    local floor = area.height - 120
    graphics2d.drawRect(area, '#FF1E2A3A')
    graphics2d.drawRect({0, floor + 30, area.width, 90}, '#FF2A3A2A')
    graphics2d.drawRect({area.width * self.platform.x - 120, floor + 20, 240, 24}, '#FFFF9040')
    for _, dot in ipairs(self.dots) do
        graphics2d.drawCircle(area.width * dot.x, dot.y, 18, '#FF60C0FF')
    end
    Test.caption(string.format('Timer "always" %.1f', self.always), area.width * 0.6, 60, {size = 48, color = Test.ink})
end

return Pause
