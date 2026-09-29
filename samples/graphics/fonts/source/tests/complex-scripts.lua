-- Complex scripts: every run is shaped by HarfBuzz in its script, language and direction, so Arabic letters join, Devanagari forms conjuncts and places its vowel signs, and Thai stacks its marks. The bidirectional algorithm orders every line for display, so a right-to-left paragraph keeps English words and numbers in their own order. Lines wrap by the Unicode rules, and Thai, which writes no spaces, between the phrases the Thai model finds.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local fonts = require('fonts')
local sample = require('sample')

local ComplexScripts = haylen.class('ComplexScripts', sample.Test)

ComplexScripts.hints = 'Drag the width slider to wrap the paragraphs again, pick the direction of the mixed line, and replay the right-to-left typewriter.'
ComplexScripts.focus = 'width'

local kLines = {
    {'Arabic', 'مرحبا بالعالم، هذه لغة جميلة', 'ar'},
    {'Persian', 'سلام دنیا، زبان فارسی زیباست', 'fa'},
    {'Urdu', 'اردو ایک خوبصورت زبان ہے', 'ur'},
    {'Hebrew', 'שלום עולם, ברוכים הבאים', 'he'},
    {'Hindi', 'नमस्ते दुनिया, क्षत्रिय और हिन्दी', 'hi'},
    {'Thai', 'สวัสดีชาวโลก ภาษาไทยสวยงาม', 'th'},
}
local kMixed = 'السعر 42 دولارًا (USD) مع كلمة English'
local kParagraph = '[b]الرحلة[/b] بدأت في الصباح الباكر، وكان [color=#FFF2B23A]البحر[/color] هادئًا. قرأ القبطان الرسالة رقم 1024 من [u]Harbor Station[/u] ثم أبحر نحو الجزيرة.'
local kThai = 'ภาษาไทยไม่มีช่องว่างระหว่างคำ จึงตัดบรรทัดตามวลีที่แบบจำลองภาษาไทยหาได้'
local kTyped = '[p dir=rightToLeft][b]القبطان:[/b] مرحبًا أيها المسافر.[pause=0.5] [speed=0.5]البحر هادئ اليوم.[/speed][/p]'

function ComplexScripts:init(entry)
    ComplexScripts.super.init(self, entry)
    self.width = 460
    self.direction = 'auto'
    local family = fonts.family('scripts')
    self.paragraph = graphics2d.newRichText(kParagraph, {family = family, size = 30, maxWidth = self.width, lineSpacing = 1, direction = 'rightToLeft', language = 'ar'})
    self.typed = graphics2d.newRichText(kTyped, {family = family, size = 30, maxWidth = 600, lineSpacing = 1, revealSpeed = 14})
end

function ComplexScripts:controls()
    return {
        ui.label{text = 'Paragraph width'},
        ui.slider{id = 'width', min = 240, max = 600, step = 10, value = self.width, showValue = true, decimals = 0, onChange = function(event)
            self.width = event.value
            self.paragraph.maxWidth = event.value
        end},
        ui.label{text = 'Mixed line direction'},
        ui.segmentedControl{id = 'direction', selected = self.direction, items = {{id = 'auto', text = 'Auto'}, {id = 'leftToRight', text = 'LTR'}, {id = 'rightToLeft', text = 'RTL'}}, onChange = function(event)
            self.direction = event.value
        end},
        ui.button{id = 'replay', text = 'Replay the typewriter', onClick = function()
            self.typed.visibleCharacters = 0
        end},
    }
end

function ComplexScripts:update(dt)
    self.paragraph:update(dt)
    self.typed:update(dt)
    self:setStatus(string.format('%d lines of Arabic, %d of %d characters revealed', self.paragraph:frame().lineCount, self.typed.visibleCharacters, self.typed.characterCount))
end

function ComplexScripts:render()
    local stage = self:stage()
    if stage == nil then
        return
    end
    local family = fonts.family('scripts')
    graphics2d.beginScreen()
    local y = stage.y
    for _, line in ipairs(kLines) do
        sample.caption(line[1], stage.x, y + 8)
        graphics2d.drawText(family, line[2], stage.x + 130, y, {size = 32, language = line[3]})
        y = y + 64
    end

    -- The mixed line reads in the direction the panel picks, and auto takes it from its first Arabic letter.
    local mixed = {size = 30, maxWidth = 560, lineSpacing = 1, direction = self.direction}
    local _, mixedHeight = graphics2d.measureText(family, kMixed, mixed)
    sample.caption('Mixed, ' .. self.direction, stage.x, y + 8)
    graphics2d.drawRectOutline({stage.x + 130, y, 560, mixedHeight}, 2, sample.guide)
    graphics2d.drawText(family, kMixed, stage.x + 130, y, mixed)

    local right = stage.x + 760
    sample.caption('Right-to-left rich text with spans', right, stage.y)
    local _, height = self.paragraph:size()
    self.paragraph:draw(right, stage.y + 34)
    graphics2d.drawRectOutline({right, stage.y + 34, self.width, height}, 2, sample.guide)

    local thaiTop = stage.y + 60 + height
    sample.caption('Thai wraps between phrases', right, thaiTop)
    graphics2d.drawText(family, kThai, right, thaiTop + 34, {size = 30, maxWidth = self.width, lineSpacing = 1, language = 'th'})

    local typedTop = stage:bottom() - 90
    sample.caption('A typewriter reveals in reading order, from the right', right, typedTop)
    self.typed:draw(right, typedTop + 34)
end

return ComplexScripts
