-- Gradient text: the text shader fills the glyphs of a distance field font with colors that blend from stop to stop across each line, from the top down unless the gradient names an angle. The gradient turns and stretches with the text, rich text fills the run of a gradient tag, and a label of the interface takes one as a property.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local GradientText = haylen.class('GradientText', TextTest)

GradientText.code = [[
local gold = {stops = {{0, '#FFFFF4C2'}, {0.45, '#FFF6C26B'}, {0.55, '#FFC8861E'}, {1, '#FF7A4A10'}}}
graphics2d.drawText(font, 'Treasure', x, y, {size = 120, gradient = gold, outlineWidth = 6, outlineColor = '#FF2A1600'})
graphics2d.drawText(font, '-128', x, y, {size = 72, rotation = angle, gradient = {colors = {'#FFFFE066', '#FFFF3B30'}, angle = 0}})
graphics2d.newRichText('[gradient=#FF7FE3FF,#FF2E5BFF]Frost[/gradient] bolt', {family = family, size = 64})
ui.label{text = 'Level up', font = 'title', gradient = {colors = {'#FFB8F28C', '#FF2E9E4F'}}}]]

GradientText.gold = {stops = {{0, '#FFFFF4C2'}, {0.45, '#FFF6C26B'}, {0.55, '#FFC8861E'}, {1, '#FF7A4A10'}}}
GradientText.fire = {colors = {'#FFFFE066', '#FFFF3B30'}, angle = 0}
GradientText.dusk = {colors = {'#FFFFD6E8', '#FFB57BFF', '#FF4B3BC9'}}

function GradientText:init(entry)
    GradientText.super.init(self, entry)
    self.font = fonts.load('lilita')
    self.family = fonts.family('lilita')
    self.time = 0
    self.spin = true
    self.rich = graphics2d.newRichText('A [gradient=#FF7FE3FF,#FF2E5BFF]Frost[/gradient] bolt and a [gradient=#FFFFE066,#FFFF6A00,#FFB3001B offsets=0,0.5,1 angle=0.6]Fire[/gradient] bolt', {family = self.family, size = 56, color = '#FFE8EAF2'})
end

function GradientText:enter()
    self:frame{
        code = GradientText.code,
        hint = 'Each line of the title shows the whole gradient, the damage number turns with its horizontal gradient, and plain text beside the gradients keeps its color.',
        controls = {
            ui.label{text = 'Level up', font = 'title', gradient = {colors = {'#FFB8F28C', '#FF2E9E4F'}}},
            ui.label{text = 'A label of the interface fills with the gradient of its property.', color = 'textMuted'},
            ui.toggle{id = 'spin', text = 'Turn the damage number', checked = self.spin, onChange = function(event)
                self.spin = event.checked
            end},
        },
        focus = 'spin',
    }
end

function GradientText:update(dt)
    GradientText.super.update(self, dt)
    self.rich:update(dt)
    if self.spin then
        self.time = self.time + dt
    end
end

function GradientText:draw(area)
    local layout = self.layout
    local font = self.font
    local half = layout.width / 2
    TextTest.fill('#FF161B28')

    TextTest.label('Four stops with an outline, and each line fills on its own', 24, 0)
    graphics2d.drawText(font, 'Treasure\nchest', 24, 44, {size = 120, lineSpacing = 1, gradient = GradientText.gold, outlineWidth = 6, outlineColor = '#FF2A1600', shadowOffset = {6, 8}, shadowColor = '#A0000000', shadowBlur = 6})

    TextTest.label('A horizontal gradient turns with the text', half + 24, 0)
    local angle = math.sin(self.time * 1.5) * 0.4
    graphics2d.drawText(font, '-128', half + 200, 150, {size = 96, anchor = {0.5, 0.5}, rotation = angle, scale = {1 + math.sin(self.time * 3) * 0.1, 1}, gradient = GradientText.fire, outlineWidth = 4, outlineColor = '#FF3A0A00'})
    graphics2d.drawText(font, 'Plain', half + 420, 110, {size = 72, color = '#FFE8EAF2'})

    local bottom = layout.height * 0.58
    TextTest.label('Rich text fills the runs of its gradient tags', 24, bottom)
    self.rich:draw(24, bottom + 44)
    TextTest.label('Three colors from the top down', half + 24, bottom)
    graphics2d.drawText(font, 'Dusk', half + 24, bottom + 44, {size = 96, gradient = GradientText.dusk})
end

return GradientText
