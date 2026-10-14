-- Batches of several textures: a meadow of heroes, rocks, trees and bunnies from four separate images in one sprite batch that draws them in one call, with a material that sways each sprite by the phase, strength and brightness of its own sprite data.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')

local BatchTextures = haylen.class('BatchTextures', SpriteTest)

BatchTextures.images = {'hero', 'rock', 'tree', 'bunny'}
BatchTextures.count = 1500

BatchTextures.code = [[
local meadow = graphics2d.newSpriteBatch({hero, rock, tree, bunny})
meadow:add({x = 100, y = 200, texture = 3, data = {phase, strength, brightness}})
meadow:draw({material = sway})  -- One draw call for the four textures.]]

function BatchTextures:enter()
    self.textures = {}
    for index, name in ipairs(BatchTextures.images) do
        self.textures[index] = SpriteTest.texture('images/' .. name .. '.png')
    end
    self.sway = graphics2d.newMaterial(assets.shader('shaders/sway.shader'))
    self.meadow = graphics2d.newSpriteBatch(self.textures)
    self.meadow:reserve(BatchTextures.count)
    self.swaying = true
    self.time = 0
    self:frame{
        code = BatchTextures.code,
        hint = 'Every sprite picks one of four textures and sways by its own data, and the whole meadow stays one draw call.',
        controls = {ui.toggle{id = 'sway', text = 'Sway each sprite by its data', checked = true, onChange = function(event) self.swaying = event.checked end}},
        focus = 'sway',
    }
end

-- Scatters the meadow over the stage, each sprite with its texture, a size from it and its own phase, strength and brightness.
function BatchTextures:resize(area)
    self.meadow:clear()
    for index = 1, BatchTextures.count do
        local which = (index - 1) % #self.textures + 1
        local texture = self.textures[which]
        local scale = 0.18 + math.random() * 0.14
        self.meadow:add({
            x = math.random() * area.width,
            y = math.random() * area.height,
            width = texture.width * scale,
            height = texture.height * scale,
            pivotY = 1,
            texture = which,
            data = {math.random() * math.pi * 2, 0.04 + math.random() * 0.08, math.random() * 0.25},
        })
    end
end

function BatchTextures:update(dt)
    BatchTextures.super.update(self, dt)
    self.time = self.time + dt
    self.sway:set('time', self.time)
    local stats = graphics2d.stats()
    self:status(string.format('Sprites %d from %d textures   Draw calls %d   Texture switches %d', self.meadow:size(), #self.textures, stats.drawCalls, stats.textureSwitches))
end

function BatchTextures:draw()
    self.meadow:draw(self.swaying and {material = self.sway} or nil)
end

return BatchTextures
