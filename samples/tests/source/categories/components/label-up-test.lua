-- Label up: the component of `label-up.lua` over a knight that takes hits, criticals, heals, gold and misses, from the buttons, from a click or a tap at the cursor and by itself, with labels that stack when they come in quick series and a burst that fills the pool.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ComponentTest = require('categories.components.component-test')
local LabelUp = require('categories.components.label-up')

local LabelUpTest = haylen.class('LabelUpTest', ComponentTest)

LabelUpTest.font = 'fonts/lilita-one-regular.ttf'

LabelUpTest.code = [[
local LabelUp = require('label-up')  -- One file, copied from samples/tests.
local labels = LabelUp.new({font = assets.font('fonts/lilita-one-regular.ttf'), size = 64, rise = 150})
labels:spawn(enemy.x, enemy.y - 180, '-' .. damage, 'damage')  -- Or 'critical', 'heal', 'gold', 'miss'.
labels:update(dt)
labels:draw()]]

function LabelUpTest:enter()
    LabelUpTest.super.enter(self)
    self.labels = LabelUp.new({font = assets.font(LabelUpTest.font), size = 64, rise = 150, life = 0.9})
    self.knight = ComponentTest.target('hero', 0, 300, 1.6)
    self.automatic = true
    self.wait = 0
    self.dt = 0
    self:frame{
        hint = 'Click, tap or press E or the south button to hit the knight where the cursor is, or press a button. The labels of a quick series stack upward.',
        code = LabelUpTest.code,
        play = true,
        view = ComponentTest.view,
        controls = {
            ui.row{wrap = true, children = {
                ui.button{id = 'damage', text = 'Hit', variant = 'primary', onClick = function() self:show('damage') end},
                ui.button{id = 'critical', text = 'Critical', onClick = function() self:show('critical') end},
                ui.button{id = 'heal', text = 'Heal', onClick = function() self:show('heal') end},
                ui.button{id = 'gold', text = 'Gold', onClick = function() self:show('gold') end},
                ui.button{id = 'miss', text = 'Miss', onClick = function() self:show('miss') end},
            }},
            ui.button{id = 'burst', text = 'A burst of 60 labels', onClick = function() self:burst() end},
            ui.toggle{id = 'automatic', text = 'Hits by themselves', checked = true, onChange = function(event) self.automatic = event.checked end},
            ui.label{text = 'Damage pops in red, criticals large in gold, heals in green, gold in yellow and misses in gray, each with a dark outline that reads over any ground.', color = 'textMuted', font = 'caption'},
        },
    }
end

-- Shows a label of a kind over the head of the knight, with a value that suits it.
function LabelUpTest:show(kind, x, y)
    local texts = {damage = '-' .. math.random(6, 18), critical = '-' .. math.random(40, 70) .. '!', heal = '+' .. math.random(5, 25), gold = '+' .. math.random(1, 9) .. ' gold', miss = 'Miss'}
    if kind ~= 'heal' and kind ~= 'gold' then
        ComponentTest.hit(self.knight)
    end
    self.labels:spawn(x or self.knight.x, y or self.knight.y - self.knight.height - 10, texts[kind], kind)
end

function LabelUpTest:burst()
    for index = 1, 60 do
        local angle = index / 60 * math.pi * 2
        self.labels:spawn(math.cos(angle) * 520, -60 + math.sin(angle) * 300, tostring(index), index % 7 == 0 and 'critical' or 'damage')
    end
end

function LabelUpTest:update(dt)
    LabelUpTest.super.update(self, dt)
    self.dt = dt
    if not self.stage then
        return
    end

    if self:pressed() then
        self:show(math.random() < 0.2 and 'critical' or 'damage', self.pointer.worldX, self.pointer.worldY)
    end
    if self.automatic then
        self.wait = self.wait - dt
        if self.wait <= 0 then
            self.wait = 0.55
            local roll = math.random()
            self:show(roll < 0.55 and 'damage' or roll < 0.7 and 'critical' or roll < 0.82 and 'heal' or roll < 0.92 and 'gold' or 'miss')
        end
    end
    self.labels:update(dt)
    self:status(string.format('Labels on screen %d, records in the pool %d, draw calls %d', self.labels.count, #self.labels.records, graphics2d.stats().drawCalls))
end

function LabelUpTest:draw(area)
    self:drawGround()
    self:drawTarget(self.knight, self.dt)
    self.labels:draw()
    ComponentTest.name('A knight that takes the hits', 0, ComponentTest.groundLine + 36)
end

return LabelUpTest
