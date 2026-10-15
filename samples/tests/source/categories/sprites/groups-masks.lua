-- Groups and masks: a round portrait and a shaped card drawn through masks with smooth edges, fog of war that a soft hole under the pointer cuts open with an outside mask, and the same overlapping shapes faded one by one beside a group that fades them as one image.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')

local GroupsMasks = haylen.class('GroupsMasks', SpriteTest)

GroupsMasks.code = [[
graphics2d.pushMask(function() graphics2d.drawCircle(x, y, 64, '#FFFFFF') end)
graphics2d.draw(hero, x, y)  -- Shows only inside the circle.
graphics2d.popMask()
graphics2d.beginGroup({opacity = 0.5})  -- Overlaps fade as one image.]]

function GroupsMasks:enter()
    self.hero = SpriteTest.texture('images/hero.png')
    self.tiles = SpriteTest.texture('images/tiles.png')
    self.opacity = 0.5
    self.time = 0
    self:frame{
        code = GroupsMasks.code,
        hint = 'Move the pointer over the fog on the right to light it, and change the opacity of the faded shapes.',
        controls = {
            ui.formField{label = 'Opacity', ui.slider{id = 'opacity', min = 0, max = 1, value = self.opacity, step = 0.05, showValue = true, onChange = function(event) self.opacity = event.value end}},
        },
        focus = 'opacity',
    }
end

function GroupsMasks:update(dt)
    GroupsMasks.super.update(self, dt)
    self.time = self.time + dt
    self.pointerX, self.pointerY = self:pointer()
    local stats = graphics2d.stats()
    self:status(string.format('Canvases %d   Passes %d   Draw calls %d', stats.canvases, stats.passes, stats.drawCalls))
end

-- Overlapping shapes whose overlaps show when each one fades by itself.
function GroupsMasks:shapes(x, y, alpha)
    local color = string.format('#%02XFF8A3D', math.floor(alpha * 255 + 0.5))
    graphics2d.drawCircle(x, y, 46, color)
    graphics2d.drawCircle(x + 52, y, 46, color)
    graphics2d.drawShape({x - 20, y + 10, 92, 60}, {radius = 14, color = color})
end

function GroupsMasks:draw(area)
    local width, height = area.width, area.height

    -- A round portrait that pulses and a card with rounded corners whose stripes stay inside it.
    local portraitX, portraitY = width * 0.14, height * 0.32
    local radius = 70 + math.sin(self.time * 2) * 8
    graphics2d.pushMask(function()
        graphics2d.drawCircle(portraitX, portraitY, radius, '#FFFFFF')
    end)
    graphics2d.drawRect({portraitX - 90, portraitY - 90, 180, 180}, '#FF3D5A80')
    graphics2d.draw(self.hero, portraitX, portraitY)
    graphics2d.popMask()
    graphics2d.drawRing(portraitX, portraitY, radius, 4, '#FFFFD54F')

    local cardX, cardY = width * 0.04, height * 0.62
    graphics2d.pushMask(function()
        graphics2d.drawShape({cardX, cardY, width * 0.26, height * 0.3}, {radius = 28, color = '#FFFFFF'})
    end)
    for stripe = 0, 14 do
        graphics2d.drawLine(cardX + stripe * 30 - 120, cardY - 20, cardX + stripe * 30 + 40, cardY + height * 0.32, 12, stripe % 2 == 0 and '#FF29B6F6' or '#FF7E57C2')
    end
    graphics2d.popMask()

    -- Fog of war over a field of tiles, cut open by a soft circle around the pointer.
    local fogX, fogY, fogWidth, fogHeight = width * 0.36, height * 0.05, width * 0.3, height * 0.9
    graphics2d.pushClip({fogX, fogY, fogWidth, fogHeight})
    for row = 0, math.floor(fogHeight / 48) do
        for column = 0, math.floor(fogWidth / 48) do
            graphics2d.draw(self.tiles, fogX + column * 48, fogY + row * 48, {source = {((row + column) % 3) * 64, 0, 64, 64}, width = 48, height = 48, pivotX = 0, pivotY = 0})
        end
    end
    local holeX = math.max(fogX, math.min(fogX + fogWidth, self.pointerX or fogX + fogWidth / 2))
    local holeY = math.max(fogY, math.min(fogY + fogHeight, self.pointerY or fogY + fogHeight / 2))
    graphics2d.pushMask(function()
        graphics2d.drawShape({holeX - 90, holeY - 90, 180, 180}, {radius = 90, softness = 60, color = '#FFFFFF'})
    end, {mode = 'outside', layer = 1})
    graphics2d.drawRect({fogX, fogY, fogWidth, fogHeight}, '#F0101820')
    graphics2d.popMask()
    graphics2d.popClip()

    -- The same shapes faded one by one, whose overlaps show, and faded as one group.
    local shapesX = width * 0.72
    self:shapes(shapesX, height * 0.25, self.opacity)
    graphics2d.beginGroup({opacity = self.opacity})
    self:shapes(shapesX, height * 0.65, 1)
    graphics2d.endGroup()
    graphics2d.drawText(nil, 'One by one', shapesX + 26, height * 0.25 + 90, {size = 20, color = '#FFFFFFFF', align = 'center'})
    graphics2d.drawText(nil, 'As one group', shapesX + 26, height * 0.65 + 90, {size = 20, color = '#FFFFFFFF', align = 'center'})
end

return GroupsMasks
