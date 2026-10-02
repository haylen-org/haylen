-- Custom effects: a transition effect is a table with a `render` method that draws the images of both scenes itself, and switch and exit points that say when the stack changes and when the leaving scene exits.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Card = require('card')
local sample = require('sample')

local CustomEffect = haylen.class('CustomEffect', sample.Test)

CustomEffect.hints = 'Push a card with either effect. Both show the two scenes during the whole transition, so they switch at 0 and exit at 1.'

-- Blinds: the incoming scene opens in horizontal bands, from the top band to the bottom one.
CustomEffect.blinds = {
    switchProgress = 0,
    exitProgress = 1,
    render = function(self, progress, outgoing, incoming)
        graphics2d.beginScreen()
        local area = graphics2d.canvasBounds()
        graphics2d.draw(outgoing, area.x, area.y, {pivotX = 0, pivotY = 0, width = area.width, height = area.height})
        local bands = 10
        for index = 0, bands - 1 do
            local open = m.clamp(progress * 1.8 - index / bands * 0.8, 0, 1)
            if open > 0 then
                local share = open / bands
                local center = (index + 0.5) / bands
                graphics2d.draw(incoming, area.x, area.y + area.height * (center - share / 2), {pivotX = 0, pivotY = 0, width = area.width, height = area.height * share, source = {0, incoming.height * (center - share / 2), incoming.width, incoming.height * share}, layer = 1})
            end
        end
    end,
}

-- Curtain: the outgoing scene splits in two halves that slide apart over the incoming one, which grows into place.
CustomEffect.curtain = {
    switchProgress = 0,
    exitProgress = 1,
    render = function(self, progress, outgoing, incoming)
        graphics2d.beginScreen()
        local area = graphics2d.canvasBounds()
        local scale = 0.85 + 0.15 * progress
        local centerX, centerY = area.x + area.width / 2, area.y + area.height / 2
        graphics2d.draw(incoming, centerX, centerY, {width = area.width * scale, height = area.height * scale})
        local half = area.width / 2
        local shift = half * progress
        graphics2d.draw(outgoing, area.x - shift, area.y, {pivotX = 0, pivotY = 0, width = half, height = area.height, source = {0, 0, outgoing.width / 2, outgoing.height}, layer = 1})
        graphics2d.draw(outgoing, area.x + half + shift, area.y, {pivotX = 0, pivotY = 0, width = half, height = area.height, source = {outgoing.width / 2, 0, outgoing.width / 2, outgoing.height}, layer = 1})
    end,
}

function CustomEffect:init(entry)
    CustomEffect.super.init(self, entry)
    self.pushed = 0
end

function CustomEffect:controls()
    return {
        ui.button{text = 'Push with blinds', onClick = function() self:push('blinds') end},
        ui.button{text = 'Push with the curtain', onClick = function() self:push('curtain') end},
    }
end

function CustomEffect:pause()
    self.header.visible = false
end

function CustomEffect:resume()
    self.header.visible = true
end

function CustomEffect:push(name)
    if scene.transitioning() then
        return
    end
    local transition = {effect = CustomEffect[name], duration = 1.1, ease = 'cubicInOut'}
    self.pushed = self.pushed + 1
    scene.push(Card({title = 'Effect "' .. name .. '"', caption = 'A Lua transition effect', color = self.pushed % 2 == 0 and '#FF3E6E8A' or '#FF8A4E3E', stay = 1.2, leave = transition}), transition)
    self:setStatus(string.format('%d cards pushed with custom effects', self.pushed))
end

function CustomEffect:render()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect(area, '#FF222A20')
    local time = haylen.elapsed()
    for index = 0, 7 do
        local x = area.x + area.width * (index + 0.5) / 8
        graphics2d.drawCircle(x, area.y + area.height * 0.6 + math.sin(time * 2 + index) * 80, 50, '#FF5A8A4A')
    end
end

return CustomEffect
