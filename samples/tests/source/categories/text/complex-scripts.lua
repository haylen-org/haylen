-- Complex scripts: every run is shaped in its script, language and direction, so Arabic letters join, Devanagari forms conjuncts and places its vowel signs, and Thai stacks its marks. The bidirectional algorithm orders every line for display, so a right-to-left paragraph keeps English words and numbers in their own order. Lines wrap by the Unicode rules, and Thai, which writes no spaces, between the phrases a model of the language finds.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local ComplexScripts = haylen.class('ComplexScripts', TextTest)

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
ComplexScripts.directions = {{id = 'auto', text = 'Auto'}, {id = 'leftToRight', text = 'Left to right'}, {id = 'rightToLeft', text = 'Right to left'}}

function ComplexScripts:init(entry)
    ComplexScripts.super.init(self, entry)
    self.width = 460
    self.direction = 'auto'
    self.family = fonts.family('scripts')
    self.paragraph = graphics2d.newRichText(ComplexScripts.paragraph, {family = self.family, size = 30, maxWidth = self.width, lineSpacing = 1, direction = 'rightToLeft', language = 'ar'})
    self.typed = graphics2d.newRichText(ComplexScripts.typed, {family = self.family, size = 30, maxWidth = 600, lineSpacing = 1, revealSpeed = 14})
end

function ComplexScripts:enter()
    self:frame{
        hint = 'Drag the width slider to wrap the paragraphs again, pick the direction of the mixed line, and replay the right-to-left typewriter.',
        controls = {
            ui.formField{label = 'Paragraph width', ui.slider{id = 'width', min = 240, max = 600, step = 10, value = self.width, showValue = true, decimals = 0, onChange = function(event)
                self.width = event.value
                self.paragraph.maxWidth = event.value
            end}},
            ui.formField{label = 'Mixed line direction', ui.segmentedControl{id = 'direction', selected = self.direction, items = ComplexScripts.directions, onChange = function(event)
                self.direction = event.value
            end}},
            ui.button{id = 'replay', text = 'Replay the typewriter', onClick = function()
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
    self:status(string.format('Arabic lines %d   Revealed %d of %d characters', self.paragraph:frame().lineCount, self.typed.visibleCharacters, self.typed.characterCount))
end

function ComplexScripts:draw(area)
    local family, layout = self.family, self.layout
    local y = 0
    for _, line in ipairs(ComplexScripts.lines) do
        Test.caption(line[1], 0, y + 8, {size = 22})
        graphics2d.drawText(family, line[2], 130, y, {size = 32, language = line[3]})
        y = y + 64
    end

    -- The mixed line reads in the direction the panel picks, and "auto" takes it from its first Arabic letter.
    local mixed = {size = 30, maxWidth = 560, lineSpacing = 1, direction = self.direction}
    local _, mixedHeight = graphics2d.measureText(family, ComplexScripts.mixed, mixed)
    Test.caption('Mixed, "' .. self.direction .. '"', 0, y + 8, {size = 22})
    graphics2d.drawRectOutline({130, y, 560, mixedHeight}, 2, Test.line)
    graphics2d.drawText(family, ComplexScripts.mixed, 130, y, mixed)

    local right = 760
    Test.caption('Right-to-left rich text with spans', right, 0, {size = 22})
    local _, height = self.paragraph:size()
    self.paragraph:draw(right, 34)
    graphics2d.drawRectOutline({right, 34, self.width, height}, 2, Test.line)

    local thaiTop = 60 + height
    Test.caption('Thai wraps between phrases', right, thaiTop, {size = 22})
    graphics2d.drawText(family, ComplexScripts.thai, right, thaiTop + 34, {size = 30, maxWidth = self.width, lineSpacing = 1, language = 'th'})

    local typedTop = layout.height - 110
    Test.caption('A typewriter reveals in reading order, from the right', right, typedTop, {size = 22})
    self.typed:draw(right, typedTop + 34)
end

return ComplexScripts
