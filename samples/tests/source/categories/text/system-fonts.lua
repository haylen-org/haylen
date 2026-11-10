-- Fonts of the system: the default family of the engine holds Latin letters alone, so it draws Chinese, Japanese, Korean, Thai and Hindi, and what a person types into the field, with the fonts of the system, which the engine finds through the API of each platform and loads once each. A family made with "systemFonts = false" keeps the missing glyph box beside it, and a web page, which has no fonts of the system, shows the box on both sides.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local TextTest = require('categories.text.text-test')

local SystemFonts = haylen.class('SystemFonts', TextTest)

SystemFonts.code = [[
local family = graphics.newFontFamily({regular = graphics2d.defaultFont()})
local fixed = graphics.newFontFamily({regular = graphics2d.defaultFont(), systemFonts = false})
graphics2d.drawText(family, '東京 서울 กรุงเทพ नमस्ते', x, y, {size = 40})
graphics2d.drawText(fixed, '東京 서울 กรุงเทพ नमस्ते', x, y, {size = 40})
local names = graphics.systemFonts()]]

SystemFonts.lines = {
    {'Chinese', '我在學習中文。春夏秋冬。'},
    {'Japanese', 'ひらがなとカタカナ'},
    {'Korean', '서울에서 만나요'},
    {'Thai', 'กรุงเทพมหานคร'},
    {'Hindi', 'नमस्ते दुनिया'},
}
SystemFonts.size = 40

function SystemFonts:init(entry)
    SystemFonts.super.init(self, entry)
    self.family = graphics.newFontFamily({regular = graphics2d.defaultFont()})
    self.fixed = graphics.newFontFamily({regular = graphics2d.defaultFont(), systemFonts = false})
    self.typed = '安娜 Ana'
end

function SystemFonts:enter()
    self:frame{
        code = SystemFonts.code,
        hint = 'Type into the field with any keyboard or input method of the system. The left column draws what the default font lacks with the fonts of the system once they arrive, and the right column keeps the missing glyph box.',
        controls = {
            ui.formField{label = 'Type a name in any script', ui.textField{id = 'name', value = self.typed, onChange = function(event)
                self.typed = event.value
            end}},
            ui.label{text = 'The field and this panel draw with the families of the interface, which use the fonts of the system too.', color = 'textMuted'},
        },
        focus = 'name',
    }
end

function SystemFonts:update(dt)
    SystemFonts.super.update(self, dt)
    local names = graphics.systemFonts()
    if #names == 0 then
        self:status('No font of the system arrived yet, and a platform without fonts of the system, such as the web, never gives one.')
    else
        self:status(string.format('%d faces of the system arrived: %s.', #names, table.concat(names, ', ')))
    end
end

function SystemFonts:draw(area)
    local layout = self.layout
    local half = layout.width / 2
    local size = SystemFonts.size
    TextTest.fill('#FF161B28')
    TextTest.label('With the fonts of the system', 24, 0)
    TextTest.label('With "systemFonts = false"', half + 24, 0)

    local y = 50
    for _, line in ipairs(SystemFonts.lines) do
        TextTest.label(line[1], 24, y, {size = 20})
        graphics2d.drawText(self.family, line[2], 24, y + 26, {size = size})
        graphics2d.drawText(self.fixed, line[2], half + 24, y + 26, {size = size})
        y = y + size + 46
    end

    TextTest.label('What the field holds', 24, y)
    graphics2d.drawText(self.family, self.typed, 24, y + 30, {size = size * 1.4, maxWidth = half - 48})
    graphics2d.drawText(self.fixed, self.typed, half + 24, y + 30, {size = size * 1.4, maxWidth = half - 48})
end

return SystemFonts
