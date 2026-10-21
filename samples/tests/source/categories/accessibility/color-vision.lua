-- The options `colorVision` and `correctColorVision` of the post-processing of a world canvas show the canvas as a deficiency of color vision sees it, or correct for it. The same game screen draws twice side by side, as normal sight sees it and through the deficiency, with the colors that games often lean on alone: the health and stamina bars, two teams and the markers of a map.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local ColorVision = haylen.class('ColorVision', Test)

ColorVision.code = [[
graphics2d.beginWorld(normal)  -- The left half, as normal sight sees it.
graphics2d.beginWorld(seen, {postProcess = {colorVision = 'deuteranopia'}})  -- The right half, as that eye sees it.
graphics2d.beginWorld(seen, {postProcess = {colorVision = 'deuteranopia', correctColorVision = true}})  -- Corrects for it.]]

ColorVision.visions = {
    {id = 'protanopia', text = 'Protanopia, no red cones'},
    {id = 'deuteranopia', text = 'Deuteranopia, no green cones'},
    {id = 'tritanopia', text = 'Tritanopia, no blue cones'},
}
ColorVision.names = {protanopia = 'Protanopia', deuteranopia = 'Deuteranopia', tritanopia = 'Tritanopia'}
ColorVision.bars = {{'Health', 0.72, '#FFE5534B'}, {'Stamina', 0.48, '#FF3DBE7A'}, {'Mana', 0.6, '#FF4C7DFF'}}
ColorVision.teams = {{'Red team', '#FFD03B2F'}, {'Green team', '#FF3F9B3A'}}
-- The markers of the map, as shares of the map, with what each means.
ColorVision.markers = {{0.3, 0.32, '#FFD03B2F', 'Danger'}, {0.62, 0.3, '#FF3F9B3A', 'Camp'}, {0.45, 0.66, '#FFE88A2E', 'Quest'}, {0.75, 0.62, '#FF8DB33A', 'Herbs'}}

function ColorVision:enter()
    self.vision = 'deuteranopia'
    self.correct = false
    self.seen = graphics2d.newCamera()
    self.seen.anchor = 'topLeft'
    self.map = assets.texture('interface/images/island-map.png', {filter = 'linear'})
    self:frame{
        hint = 'Pick a deficiency to see the right half as it sees it, and turn the correction on to shift what it loses into colors it sees.',
        code = ColorVision.code,
        controls = {
            ui.sectionTitle{text = 'The right half sees'},
            ui.radioGroup{id = 'vision', selected = self.vision, items = ColorVision.visions, onChange = function(event) self.vision = event.value end},
            ui.toggle{text = 'Correct for it', onChange = function(event) self.correct = event.checked end},
        },
    }
end

function ColorVision:update(dt)
    ColorVision.super.update(self, dt)
    local name = ColorVision.names[self.vision]
    self:status(self.correct and string.format('The right half corrects the colors for %s, so the pairs it lost part again.', name:lower()) or string.format('The right half shows the game as %s sees it.', name:lower()))
end

-- Each half draws the same screen in its own canvas, the right one through the post-processing of the deficiency.
function ColorVision:render()
    if not self.area then
        return
    end
    local stage = self.stage
    local half = math.floor(stage.width / 2)
    self.camera.viewport = {stage.x, stage.y, half, stage.height}
    self.seen.viewport = {stage.x + half, stage.y, stage.width - half, stage.height}
    graphics2d.beginWorld(self.camera)
    self:drawHalf(half, stage.height, 'Normal sight')
    graphics2d.beginWorld(self.seen, {clear = Test.stageColor, postProcess = {colorVision = self.vision, correctColorVision = self.correct}})
    self:drawHalf(stage.width - half, stage.height, (self.correct and 'Corrected for ' or '') .. ColorVision.names[self.vision]:lower())
end

-- A game screen: a title, a map with its markers, the bars of a hero and two teams, laid out for the size of the half.
function ColorVision:drawHalf(width, height, title)
    graphics2d.drawRect({0, 0, width, height}, Test.stageColor, {layer = -10})
    local margin = 32
    Test.caption(title:sub(1, 1):upper() .. title:sub(2), width / 2, margin, {size = 30, color = Test.ink, anchor = {0.5, 0}})

    -- The map keeps the shape of its picture and leaves the bottom third for the bars and the teams.
    local mapTop = margin + 56
    local mapHeight = math.min((height - mapTop) * 0.58, (width - margin * 2) * 0.625)
    local mapWidth = mapHeight / 0.625
    local mapX = (width - mapWidth) / 2
    graphics2d.drawShape({mapX, mapTop, mapWidth, mapHeight}, {radius = 16, texture = self.map, borderWidth = 3, borderColor = '#FF1E1B33'})
    for _, marker in ipairs(ColorVision.markers) do
        local x, y = mapX + marker[1] * mapWidth, mapTop + marker[2] * mapHeight
        graphics2d.drawEllipse(x, y + 4, 12, 5, '#60000000')
        graphics2d.drawShape({x - 14, y - 34, 28, 28}, {kind = 'ellipse', color = marker[3], borderWidth = 3, borderColor = '#FF1E1B33', layer = 1})
        graphics2d.drawPolygon({{x - 8, y - 12}, {x + 8, y - 12}, {x, y}}, '#FF1E1B33', {layer = 1})
        graphics2d.drawCircle(x, y - 20, 5, '#FFFFFFFF', {layer = 2})
        graphics2d.drawText(nil, marker[4], x + 20, y - 20, {size = 22, color = '#FFFFFFFF', outlineWidth = 4, outlineColor = '#FF1E1B33', anchor = {0, 0.5}, layer = 3})
    end

    local top = mapTop + mapHeight + 28
    local barWidth = (width - margin * 3) * 0.55
    for index, bar in ipairs(ColorVision.bars) do
        local y = top + (index - 1) * 44
        Test.caption(bar[1], margin, y, {size = 22, color = Test.muted})
        local x = margin + 110
        local length = barWidth - 110
        graphics2d.drawShape({x, y + 2, length, 20}, {radius = 10, color = '#FF1B1F2C'})
        graphics2d.drawShape({x, y + 2, length * bar[2], 20}, {radius = 10, color = bar[3]})
    end
    local teamX = margin * 2 + barWidth
    for index, team in ipairs(ColorVision.teams) do
        local y = top + (index - 1) * 64 + 8
        graphics2d.drawShape({teamX, y, 48, 48}, {kind = 'ellipse', color = team[2], borderWidth = 3, borderColor = '#FF1E1B33'})
        graphics2d.drawCircle(teamX + 17, y + 17, 6, '#80FFFFFF', {layer = 1})
        Test.caption(team[1], teamX + 64, y + 24, {size = 24, color = Test.ink, anchor = {0, 0.5}})
    end
end

return ColorVision
