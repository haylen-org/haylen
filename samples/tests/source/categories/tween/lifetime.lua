-- Scene and target lifetime: a child scene owns a spinning tween that ends when the scene unloads, and targets whose tweens stop once the garbage collector frees them, which leave a crossed-out ghost where they were collected.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local scene = require('haylen.scene')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')
local timer = require('haylen.timer')

local Lifetime = haylen.class('Lifetime', TweenTest)

local kCode = [[
tween.rotate(spinner, 1, math.pi / 2, {owner = childScene, loopMode = 'incremental', repeatCount = -1})  -- Ends when the child unloads.
tween.to(target, 1, {x = 750}, {loopMode = 'yoyo', repeatCount = -1})  -- Holds the target weakly.
target = nil
collectgarbage()  -- The tween stops before it writes again.]]

-- A transparent scene over the test that owns a spinning tween and a GUI, both of which end when it unloads.
local Child = haylen.class('Child', scene.Scene)
Child.transparent = true

function Child:enter()
    self.spinner = {rotation = 0}
    self.star = TweenTest.icon('star')
    tween.rotate(self.spinner, 0.5, math.pi / 2, {owner = self, loopMode = 'incremental', repeatCount = -1, ease = 'backOut'})
    ui.mount(ui.card{anchor = 'center', gap = 16, onCancel = function() scene.pop() end,
        ui.label{text = 'A child scene owns this spinner.', font = 'heading'},
        ui.label{text = 'Close it and its tween ends with it.', color = 'textMuted'},
        ui.button{id = 'close', text = 'Close', variant = 'primary', onClick = function() scene.pop() end},
    }, {owner = self}):command('close', 'focus')
end

function Child:render()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect(area, '#99000000')
    graphics2d.draw(self.star, area.x + area.width / 2, area.y + area.height / 2 - 220, {width = 140, height = 140, rotation = self.spinner.rotation, layer = 1})
end

function Lifetime:enter()
    self.watch = setmetatable({}, {__mode = 'v'})
    self.gem, self.coin, self.star = TweenTest.icon('gem'), TweenTest.icon('coin'), TweenTest.icon('star')
    self:frame({
        hint = 'Open the child scene and close it, then spawn targets and drop them. The count follows the tweens.',
        code = kCode,
        controls = {
            ui.button{id = 'child', text = 'Open child scene', variant = 'primary', onClick = function() self:openChild() end},
            ui.button{id = 'spawn', text = 'Spawn targets', onClick = function() self:addTargets() end},
            ui.button{id = 'drop', text = 'Drop targets', variant = 'destructive', onClick = function() self:drop() end},
            ui.sectionTitle{text = 'Tweens alive'},
            ui.label{id = 'alive', text = '', font = 'title', color = 'accentText'},
        },
        focus = 'child',
    })
    -- The table target is dropped a moment after the test opens, so its crossed-out ghost shows next to the sprite that still runs.
    timer.after(1.2, function() self:dropTable() end, {owner = self})
end

-- The test keeps the child scene weakly, only to show its spinner while it lives.
function Lifetime:openChild()
    local child = Child()
    self.watch.child = child
    scene.push(child)
end

-- The lanes of the targets, which the targets need to know where they travel.
function Lifetime:resize(area)
    self.lanes = TweenTest.lanes(area, 3)
    if not self.started then
        self.started = true
        self:addTargets()
    end
end

-- A plain table and an engine sprite, each with a tween that repeats forever and no owner.
function Lifetime:addTargets()
    local tableLane, spriteLane = self.lanes[1], self.lanes[2]
    self.table = {x = tableLane.x}
    self.sprite = graphics2d.newSprite(self.coin, {x = spriteLane.x, y = spriteLane.y + spriteLane.height / 2, width = 56, height = 56, layer = 2})
    tween.to(self.table, 1, {x = tableLane.x + tableLane.width}, {loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut'})
    tween.to(self.sprite, 1, {x = spriteLane.x + spriteLane.width}, {loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut'})
    self.watch.table, self.watch.sprite = self.table, self.sprite
    self.lastTable, self.lastSprite = nil, nil
end

-- Lets go of the table target alone and collects it, which ends its tween.
function Lifetime:dropTable()
    if not self.table then
        return
    end
    self.lastTable = self.table.x
    self.table = nil
    collectgarbage()
end

-- Lets go of both targets and collects them, which ends their tweens.
function Lifetime:drop()
    if not self.sprite then
        return
    end
    if self.table then
        self.lastTable = self.table.x
    end
    self.lastSprite = self.sprite.x
    self.table, self.sprite = nil, nil
    collectgarbage()
end

function Lifetime:update(dt)
    Lifetime.super.update(self, dt)
    self:status(string.format('The table target is %s and the sprite target is %s.', self.watch.table and 'alive' or 'collected', self.watch.sprite and 'alive' or 'collected'))
    local alive = tostring(tween.size())
    if alive ~= self.shownAlive then
        self.shownAlive = alive
        self:set('alive', {text = alive})
    end
end

-- A ghost where a collected target was, crossed out.
local function collected(icon, x, y)
    graphics2d.draw(icon, x, y, {width = 56, height = 56, color = m.color(1, 1, 1, 0.25), layer = 1})
    graphics2d.drawLine(x - 22, y - 22, x + 22, y + 22, 6, Test.red, {layer = 2})
    graphics2d.drawLine(x - 22, y + 22, x + 22, y - 22, 6, Test.red, {layer = 2})
end

function Lifetime:draw(area)
    local plain, sprite = self.watch.table, self.watch.sprite
    local tableLane, spriteLane = self.lanes[1], self.lanes[2]
    self:drawChildLane(self.lanes[3])
    local unit = {min = tableLane.x, max = tableLane.x + tableLane.width, ticks = {}}
    TweenTest.drawLane(tableLane, {title = 'Table target', caption = plain and 'Alive, and its tween runs' or 'Collected, so its tween stopped', value = plain and plain.x or tableLane.x, icon = self.gem, alpha = plain and 1 or 0, min = unit.min, max = unit.max, ticks = unit.ticks})
    TweenTest.drawLane(spriteLane, {title = 'Sprite target', caption = sprite and 'Alive, and its tween runs' or 'Collected, so its tween stopped', value = spriteLane.x, icon = self.coin, alpha = 0, min = unit.min, max = unit.max, ticks = unit.ticks})
    if sprite then
        sprite:draw()
    end
    if not plain and self.lastTable then
        collected(self.gem, self.lastTable, tableLane.y + tableLane.height / 2)
    end
    if not sprite and self.lastSprite then
        collected(self.coin, self.lastSprite, spriteLane.y + spriteLane.height / 2)
    end
end

-- The lane of the child scene shows its spinner while the scene is open, and a still star once it closed.
function Lifetime:drawChildLane(lane)
    local child = self.watch.child
    local open = child and child.spinner and scene.state(child) == 'active'
    local middle = lane.y + lane.height / 2
    local caption = open and 'Open, and its spinner turns' or (child and 'Closed, so its tween ended' or 'Open it to give it a spinner')
    graphics2d.drawText(nil, 'Child scene', lane.x - lane.label + 24, middle - 4, {size = 26, color = Test.ink, anchor = {0, 1}})
    graphics2d.drawText(nil, caption, lane.x - lane.label + 24, middle + 4, {size = 18, color = Test.muted, anchor = {0, 0}, maxWidth = lane.label - 48})
    graphics2d.drawShape({lane.x, middle - 44, 88, 88}, {radius = 20, color = '#FF1F2535', borderWidth = 2, borderColor = Test.line})
    graphics2d.draw(self.star, lane.x + 44, middle, {width = 56, height = 56, rotation = open and child.spinner.rotation or 0, color = m.color(1, 1, 1, open and 1 or 0.35)})
end

return Lifetime
