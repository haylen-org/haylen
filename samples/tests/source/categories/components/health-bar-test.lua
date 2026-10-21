-- Health bar: the component of `health-bar.lua` on three targets and a boss bar across the top: a knight with ticks every 25 points, a bunny whose bar shows only after a change and fades once it is full again, and a rock with a shield that takes the damage first, with the labels of `label-up.lua` over every hit.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ComponentTest = require('categories.components.component-test')
local HealthBar = require('categories.components.health-bar')
local LabelUp = require('categories.components.label-up')
local Test = require('harness.test')

local HealthBarTest = haylen.class('HealthBarTest', ComponentTest)

HealthBarTest.bossWidth = 940
HealthBarTest.bossPlate = {-580, -500, 1160, 128}

HealthBarTest.code = [[
local HealthBar = require('health-bar')  -- One file, copied from samples/tests.
local bar = HealthBar.new(100, {width = 210, height = 26, tick = 25, shield = 0, hideWhenFull = false})
bar:damage(18)  -- The shield takes it first, then the health.
bar:heal(10)
bar:setShield(40)
bar:update(dt)
bar:draw(enemy.x, enemy.y - 220)]]

function HealthBarTest:enter()
    HealthBarTest.super.enter(self)
    self.labels = LabelUp.new({size = 34, rise = 90})
    self.targets = {
        ComponentTest.target('hero', -560, 300, 1.4),
        ComponentTest.target('bunny', 0, 300, 2.4),
        ComponentTest.target('rock', 560, 300, 2.2),
    }
    self.targets[1].name, self.targets[1].bar = 'Knight', HealthBar.new(100, {width = 210, height = 26, tick = 25})
    self.targets[2].name, self.targets[2].bar = 'Bunny', HealthBar.new(40, {width = 160, height = 22, tick = 0, hideWhenFull = true})
    self.targets[3].name, self.targets[3].bar = 'Rock', HealthBar.new(300, {width = 230, height = 26, tick = 50, shield = 80})
    self.boss = HealthBar.new(1000, {width = HealthBarTest.bossWidth, height = 32, tick = 100, layer = 4})
    self.slime = assets.load('sprites/atlases/slime.json', 'atlas', {filter = 'linear'})
    -- The knight and the boss open wounded, so the colors of the share left show at once.
    self.targets[1].bar:setHealth(64)
    self.boss:setHealth(720)
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
    self:drawGround()
    for _, target in ipairs(self.targets) do
        self:drawTarget(target, self.dt)
        target.bar:draw(target.x, target.y - target.height - 24)
        ComponentTest.name(target.name, target.x, ComponentTest.groundLine + 36)
    end
    self:drawBoss()
    self.labels:draw()
end

-- The boss bar sits on a plate across the top with the portrait and the name of the boss.
function HealthBarTest:drawBoss()
    local plate = HealthBarTest.bossPlate
    graphics2d.drawShape({plate[1], plate[2] + 8, plate[3], plate[4]}, {radius = 28, color = '#60000000', softness = 18})
    graphics2d.drawShape(plate, {radius = 28, color = '#F01E2232', borderWidth = 3, borderColor = '#FF3A4160', layer = 1})
    local cx, cy = plate[1] + 70, plate[2] + plate[4] / 2
    graphics2d.drawShape({cx - 54, cy - 54, 108, 108}, {kind = 'ellipse', color = '#FF2E5A3A', borderWidth = 5, borderColor = '#FFFFC93C', layer = 2})
    graphics2d.draw(self.slime.texture, cx, cy + 36, {source = self.slime:source('slime 0'), width = 84, height = 84, pivotY = 1, layer = 3})
    graphics2d.drawText(nil, 'Slime King', plate[1] + 150, plate[2] + 18, {size = 32, color = Test.ink, layer = 3})
    graphics2d.drawText(nil, string.format('%d / %d', math.ceil(self.boss.health), self.boss.maximum), plate[1] + plate[3] - 34, plate[2] + 22, {size = 26, color = '#FFAAB4CC', anchor = {1, 0}, layer = 3})
    self.boss:draw(plate[1] + 150 + HealthBarTest.bossWidth / 2, plate[2] + plate[4] - 34)
end

return HealthBarTest
