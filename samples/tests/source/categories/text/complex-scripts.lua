-- Complex scripts: every run is shaped in its script, language and direction, so Arabic letters join, Devanagari forms conjuncts and places its vowel signs, and Thai stacks its marks. The bidirectional algorithm orders every line for display, so a right-to-left paragraph keeps English words and numbers in their own order. Lines wrap by the Unicode rules, and Thai, which writes no spaces, between the phrases a model of the language finds.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local ComplexScripts = haylen.class('ComplexScripts', TextTest)

ComplexScripts.code = [[
local family = graphics.newFontFamily({regular = fira, fallbacks = {arabic, hebrew, devanagari, thai}})
graphics2d.drawText(family, 'مرحبا بالعالم', x, y, {size = 30, language = 'ar'})
graphics2d.drawText(family, mixed, x, y, {size = 30, maxWidth = 560, direction = 'rightToLeft'})
local paragraph = graphics2d.newRichText(markup, {family = family, size = 30, maxWidth = 460, language = 'ar'})
local typed = graphics2d.newRichText(dialogue, {family = family, size = 30, revealSpeed = 14})]]

ComplexScripts.lines = {
    {'Arabic', 'مرحبا بالعالم، هذه لغة جميلة', 'ar'},
    {'Persian', 'سلام دنیا، زبان فارسی زیباست', 'fa'},
    {'Urdu', 'اردو ایک خوبصورت زبان ہے', 'ur'},
    {'Hebrew', 'שלום עולם, ברוכים הבאים', 'he'},
    {'Hindi', 'नमस्ते दुनिया, क्षत्रिय और हिन्दी', 'hi'},
    {'Thai', 'สวัสดีชาวโลก ภาษาไทยสวยงาม', 'th'},
}
ComplexScripts.mixed = 'السعر 42 دولارًا (USD) مع كلمة English'
ComplexScripts.paragraph = '[b]الرحلة[/b] بدأت في الصباح الباكر، وكان [color=#FFF2B23A]البحر[/color] هادئًا. قرأ القبطان الرسالة رقم 1024 من [u]Harbor Station[/u] ثم أبحر نحو الجزيرة.'
ComplexScripts.thai = 'ภาษาไทยไม่มีช่องว่างระหว่างคำ จึงตัดบรรทัดตามวลีที่แบบจำลองภาษาไทยหาได้'
ComplexScripts.typed = '[p dir=rightToLeft][b]القبطان:[/b] مرحبًا أيها المسافر.[pause=0.5] [speed=0.5]البحر هادئ اليوم.[/speed][/p]'
ComplexScripts.directions = {{id = 'auto', text = 'From the first letter'}, {id = 'leftToRight', text = 'Left to right'}, {id = 'rightToLeft', text = 'Right to left'}}
ComplexScripts.directionNames = {auto = 'the direction of its first letter', leftToRight = 'left to right', rightToLeft = 'right to left'}
ComplexScripts.labelWidth = 150
ComplexScripts.padding = 16
ComplexScripts.caption = 40
ComplexScripts.typedWidth = 600

function ComplexScripts:init(entry)
    ComplexScripts.super.init(self, entry)
    self.width = 460
    self.direction = 'auto'
    self.family = fonts.family('scripts')
    local labelAscent = graphics2d.defaultFont():ascent(26)
    self.lines = {}
    for index, line in ipairs(ComplexScripts.lines) do
        local text = graphics2d.newRichText(line[2], {family = self.family, size = 30, language = line[3], color = Test.ink})
        local _, height = text:size()
        self.lines[index] = {label = line[1], text = text, height = height, labelY = text:frame().lines[1].baseline - labelAscent}
    end
    self.paragraph = graphics2d.newRichText(ComplexScripts.paragraph, {family = self.family, size = 30, maxWidth = self.width, lineSpacing = 1, direction = 'rightToLeft', language = 'ar', color = Test.ink})
    self.paragraphLines = self.paragraph:frame().lineCount
    self.typed = graphics2d.newRichText(ComplexScripts.typed, {family = self.family, size = 30, maxWidth = ComplexScripts.typedWidth, lineSpacing = 1, revealSpeed = 14, color = Test.ink})
end

function ComplexScripts:enter()
    self:frame{
        code = ComplexScripts.code,
        page = {1340, 660},
        hint = 'Drag the width slider to wrap the Arabic paragraph again, pick the direction of the mixed line, and replay the right-to-left typewriter.',
        controls = {
            ui.formField{label = 'Paragraph width', ui.slider{id = 'width', min = 360, max = 600, step = 10, value = self.width, showValue = true, decimals = 0, onChange = function(event)
                self.width = event.value
                self.paragraph.maxWidth = event.value
                self.paragraphLines = self.paragraph:frame().lineCount
            end}},
            ui.formField{label = 'Direction of the mixed line', ui.radioGroup{id = 'direction', selected = self.direction, items = ComplexScripts.directions, onChange = function(event)
                self.direction = event.value
            end}},
            ui.button{id = 'replay', text = 'Replay the typewriter', align = 'stretch', onClick = function()
                self.typed.visibleCharacters = 0
            end},
        },
        focus = 'width',
    }
end

function ComplexScripts:update(dt)
    ComplexScripts.super.update(self, dt)
    self.paragraph:update(dt)
    self.typed:update(dt)
    self:status(string.format('The Arabic paragraph wraps into %d lines, and the typewriter shows %d of its %d characters.', self.paragraphLines, self.typed.visibleCharacters, self.typed.characterCount))
end

-- Draws a box with its caption above it and returns the top of what it holds.
function ComplexScripts.panel(caption, x, y, width, height)
    TextTest.label(caption, x, y)
    TextTest.box({x, y + ComplexScripts.caption, width, height + ComplexScripts.padding * 2})
    return y + ComplexScripts.caption + ComplexScripts.padding
end

function ComplexScripts:draw(area)
    local family, layout, padding = self.family, self.layout, ComplexScripts.padding
    local leftWidth = math.min(700, layout.width * 0.48)
    local right = layout.width - (ComplexScripts.typedWidth + padding * 2)
    local mixed = {size = 30, maxWidth = leftWidth - padding * 2, lineSpacing = 1, direction = self.direction, color = Test.ink}
    local _, mixedHeight = graphics2d.measureText(family, ComplexScripts.mixed, mixed)
    local thaiWidth = leftWidth - padding * 2
    local thai = {size = 30, maxWidth = thaiWidth, lineSpacing = 1, language = 'th', color = Test.ink}
    local _, thaiHeight = graphics2d.measureText(family, ComplexScripts.thai, thai)

    -- The scripts take rows of one height, which share the left column with the mixed line and the Thai paragraph.
    local boxes = mixedHeight + thaiHeight + (padding * 2 + ComplexScripts.caption) * 2
    local gap = 16
    local pitch = (layout.height - boxes - gap * 2) / #self.lines
    local y = 0
    for _, line in ipairs(self.lines) do
        local top = y + (pitch - line.height) / 2
        TextTest.label(line.label, 0, top + line.labelY)
        line.text:draw(ComplexScripts.labelWidth, top)
        y = y + pitch
    end
    y = y + gap
    local top = ComplexScripts.panel('A mixed line read ' .. ComplexScripts.directionNames[self.direction], 0, y, leftWidth, mixedHeight)
    graphics2d.drawText(family, ComplexScripts.mixed, padding, top, mixed)
    y = top + mixedHeight + padding + gap
    top = ComplexScripts.panel('Thai wraps between phrases', 0, y, thaiWidth + padding * 2, thaiHeight)
    graphics2d.drawText(family, ComplexScripts.thai, padding, top, thai)

    local _, height = self.paragraph:size()
    local paragraphX = layout.width - self.width - padding * 2
    top = ComplexScripts.panel('Right-to-left rich text with spans', paragraphX, 0, self.width + padding * 2, height)
    self.paragraph:draw(paragraphX + padding, top)
    local _, typedHeight = self.typed:size(ComplexScripts.typedWidth)
    local typedTop = layout.height - typedHeight - padding * 2 - ComplexScripts.caption
    top = ComplexScripts.panel('A typewriter reveals in reading order, from the right', right, typedTop, ComplexScripts.typedWidth + padding * 2, typedHeight)
    self.typed:draw(right + padding, top)
end

return ComplexScripts
