-- Scene and target lifetime: a child scene owns a spinning tween that ends when the scene unloads, and targets whose tweens stop once the garbage collector frees them.
local graphics2d = require('haylen.graphics2d')
local graphics = require('haylen.graphics')
local haylen = require('haylen')
local scene = require('haylen.scene')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Lifetime = haylen.class('Lifetime', sample.Test)

local kCode = [[
tween.rotate(spinner, 1, math.pi / 2, {owner = childScene, loopMode = 'incremental', repeatCount = -1})  -- Ends when the child unloads.
tween.to(target, 1, {x = 750}, {loopMode = 'yoyo', repeatCount = -1})  -- Holds the target weakly.
target = nil
collectgarbage()  -- The tween stops before it writes again.]]

-- A transparent scene over the test that owns a spinning tween and a document, both of which end when it unloads.
local Child = haylen.class('Child', scene.Scene)
Child.transparent = true

function Child:enter()
    self.spinner = {rotation = 0}
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
    local x, y = area.x + area.width / 2, area.y + area.height / 2 - 220
    local points = {}
    for corner = 0, 3 do
        local turn = self.spinner.rotation + math.pi / 4 + corner * math.pi / 2
        points[corner + 1] = {x + math.cos(turn) * 70, y + math.sin(turn) * 70}
    end
    graphics2d.drawPolygon(points, sample.warm, {layer = 1})
end

function Lifetime:enter()
    self.watch = setmetatable({}, {__mode = 'v'})
    self:frame({
        hint = 'Open the child scene and close it, then spawn targets and drop them. The counts follow the tweens.',
        code = kCode,
        controls = {
            ui.button{id = 'child', text = 'Open child scene', variant = 'primary', onClick = function() scene.push(Child()) end},
            ui.button{id = 'spawn', text = 'Spawn targets', onClick = function() self:spawn() end},
            ui.button{id = 'drop', text = 'Drop targets', variant = 'destructive', onClick = function() self:drop() end},
        },
        focus = 'child',
    })
    self:spawn()
end

-- A plain table and an engine sprite, each with a tween that repeats forever and no owner.
function Lifetime:spawn()
    self.table = {x = 150}
    self.sprite = graphics2d.newSprite(graphics.whiteTexture(), {x = 150, width = 60, height = 60, color = sample.green})
    tween.to(self.table, 1, {x = 750}, {loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut'})
    tween.to(self.sprite, 1, {x = 750}, {loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut'})
    self.watch.table, self.watch.sprite = self.table, self.sprite
end

-- Lets go of both targets and collects them, which ends their tweens.
function Lifetime:drop()
    self.table, self.sprite = nil, nil
    collectgarbage()
end

function Lifetime:update(dt)
    Lifetime.super.update(self, dt)
    self:status(string.format('Tweens %d   table target %s   sprite target %s', tween.size(), self.watch.table and 'alive' or 'collected', self.watch.sprite and 'alive' or 'collected'))
end

function Lifetime:draw(area)
    local top = area.height / 2 - 80
    graphics2d.drawText(nil, 'Table target', 120, top - 70, {size = 26, color = sample.muted})
    graphics2d.drawText(nil, 'Sprite target', 120, top + 90, {size = 26, color = sample.muted})
    local plain, sprite = self.watch.table, self.watch.sprite
    if plain then
        graphics2d.drawRect({plain.x - 30, top - 30, 60, 60}, sample.accent)
    end
    if sprite then
        sprite.y = top + 160
        sprite:draw()
    end
    graphics2d.drawText(nil, string.format('%d tweens', tween.size()), area.width - 60, 60, {size = 56, color = sample.warm, anchor = {1, 0}})
end

return Lifetime
