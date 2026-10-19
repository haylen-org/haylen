-- Health bar: the component of `health-bar.lua` on three targets and a boss bar across the top: a knight with ticks every 25 points, a bunny whose bar shows only after a change and fades once it is full again, and a rock with a shield that takes the damage first, with the labels of `label-up.lua` over every hit.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ComponentTest = require('categories.components.component-test')
local HealthBar = require('categories.components.health-bar')
local LabelUp = require('categories.components.label-up')
local Test = require('harness.test')

local HealthBarTest = haylen.class('HealthBarTest', ComponentTest)

HealthBarTest.code = [[
local HealthBar = require('health-bar')  -- One file, copied from samples/tests.
local bar = HealthBar.new(100, {width = 150, height = 18, tick = 25, shield = 0, hideWhenFull = false})
bar:damage(18)  bar:heal(10)  bar:setShield(40)  if bar:isEmpty() then defeat() end
bar:update(dt)  bar:draw(enemy.x, enemy.y - 220)]]

function HealthBarTest:enter()
    HealthBarTest.super.enter(self)
    self.labels = LabelUp.new({size = 34, rise = 90})
    self.targets = {
        ComponentTest.target('hero', -560, 300, 1.4),
        ComponentTest.target('bunny', 0, 300, 2.4),
        ComponentTest.target('rock', 560, 300, 2.2),
    }
    self.targets[1].name, self.targets[1].bar = 'Knight', HealthBar.new(100, {width = 170, height = 18, tick = 25})
    self.targets[2].name, self.targets[2].bar = 'Bunny', HealthBar.new(40, {width = 130, height = 16, tick = 0, hideWhenFull = true})
    self.targets[3].name, self.targets[3].bar = 'Rock', HealthBar.new(300, {width = 190, height = 18, tick = 50, shield = 80})
    self.boss = HealthBar.new(1000, {width = 1100, height = 30, tick = 100})
    self.selected = 1
    self.dt = 0
    self:frame{
        hint = 'Click or tap a target, or press E or the south button over one, to hit it. The buttons act on the chosen target, and every hit also wears the boss bar at the top.',
        code = HealthBarTest.code,
        play = true,
        view = ComponentTest.view,
        controls = {
            ui.radioGroup{id = 'target', selected = 'knight', items = {{id = 'knight', text = 'Knight'}, {id = 'bunny', text = 'Bunny'}, {id = 'rock', text = 'Rock with a shield'}}, onChange = function(event)
                self.selected = ({knight = 1, bunny = 2, rock = 3})[event.value]
            end},
            ui.row{wrap = true, children = {
                ui.button{id = 'hit', text = 'Hit 12', variant = 'primary', onClick = function() self:strike(self.targets[self.selected], 12, false) end},
                ui.button{id = 'heavy', text = 'Heavy hit 45', onClick = function() self:strike(self.targets[self.selected], 45, true) end},
                ui.button{id = 'heal', text = 'Heal 20', onClick = function() self:mend(self.targets[self.selected], 20) end},
                ui.button{id = 'shield', text = 'Shield 40', onClick = function() self:guard(self.targets[self.selected], 40) end},
            }},
            ui.button{id = 'reset', text = 'Reset every bar', onClick = function() self:reset() end},
        },
    }
end

function HealthBarTest:labelAbove(target, text, kind)
    self.labels:spawn(target.x, target.y - target.height - 40, text, kind)
end

function HealthBarTest:strike(target, amount, critical)
    local bar = target.bar
    if bar:isEmpty() then
        return
    end
    local shielded = bar.shield
    local taken = bar:damage(amount)
    ComponentTest.hit(target)
    if taken > 0 then
        self:labelAbove(target, '-' .. math.floor(taken + 0.5), critical and 'critical' or 'damage')
    else
        self:labelAbove(target, 'Blocked', 'miss')
    end
    if shielded > 0 and bar.shield == 0 then
        self:log('The shield of the %s broke.', target.name:lower())
    end
    if bar:isEmpty() then
        self:log('The %s is down.', target.name:lower())
    end
    self.boss:damage(amount)
end

function HealthBarTest:mend(target, amount)
    target.bar:heal(amount)
    self:labelAbove(target, '+' .. amount, 'heal')
end

function HealthBarTest:guard(target, amount)
    target.bar:setShield(target.bar.shield + amount)
    self:labelAbove(target, '+' .. amount .. ' shield', 'miss')
end

function HealthBarTest:reset()
    for _, target in ipairs(self.targets) do
        target.bar:setHealth(target.bar.maximum)
    end
    self.targets[3].bar:setShield(80)
    self.boss:setHealth(self.boss.maximum)
end

function HealthBarTest:update(dt)
    HealthBarTest.super.update(self, dt)
    self.dt = dt
    if not self.stage then
        return
    end

    if self:pressed() then
        local target = self:under(self.targets)
        if target then
            self:strike(target, math.random(8, 20), false)
        end
    end
    for _, target in ipairs(self.targets) do
        target.bar:update(dt)
    end
    self.boss:update(dt)
    self.labels:update(dt)

    local knight, bunny, rock = self.targets[1].bar, self.targets[2].bar, self.targets[3].bar
    self:status(string.format('Knight %d of 100, bunny %d of 40, rock %d of 300 with shield %d, boss %d of 1000', math.ceil(knight.health), math.ceil(bunny.health), math.ceil(rock.health), math.ceil(rock.shield), math.ceil(self.boss.health)))
end

function HealthBarTest:draw(area)
    graphics2d.drawRect({-960, 300, 1920, 240}, '#FF2A3348')
    for _, target in ipairs(self.targets) do
        self:drawTarget(target, self.dt)
        target.bar:draw(target.x, target.y - target.height - 24)
        Test.caption(target.name, target.x, 340, {anchor = {0.5, 0}})
    end
    Test.caption('Boss', -560, -446, {anchor = {0, 0.5}, color = Test.ink, size = 26})
    self.boss:draw(0, -400)
    self.labels:draw()
end

return HealthBarTest
