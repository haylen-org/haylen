-- Color emoji: labels, a field, buttons and rich text take the emoji of a color font from the fallbacks of their family, in the colors of the font with the opacity of the text, while the letters around them stay sharp distance field text. Sequences joined by zero-width joiners draw as the one glyph of their ligature, the variation selectors choose between a symbol and its emoji, and a display font paints its letters with the layers of its palette.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local fonts = require('categories.text.fonts')

local ColorEmoji = haylen.class('ColorEmoji', Test)

ColorEmoji.code = [[
local emoji = assets.font('fonts/noto-color-emoji-regular.ttf')
ui.addFont('chat', graphics.newFontFamily({regular = graphics2d.defaultFont(), fallbacks = {symbols, emoji}}))
ui.setTheme(ui.addTheme({name = 'chat', fonts = {body = {font = 'chat'}, button = {font = 'chat'}}}, 'dark'))
ui.label{text = 'Party time 🎉'}
ui.textField{value = 'Ship it 🚀 👩‍💻'}
ui.richText{text = '[b]Hot[/b] 🔥 [font=layers]COLOR[/font] ❤️'}]]

ColorEmoji.hint = 'Type in the field to place the caret between emoji, which count as one character each. The sun and the heart show as symbols in the color of the text until U+FE0F asks for their emoji, and the thumb and the star show as emoji until U+FE0E asks for their symbols.'

-- The families of the text and of the code card and the color display font, registered with the UI once and kept, since the UI keeps every font it registers.
ColorEmoji.registered = false

function ColorEmoji.register()
    if ColorEmoji.registered then
        return
    end
    local fallbacks = {fonts.load('symbols'), assets.font('fonts/noto-color-emoji-regular.ttf')}
    ui.addFont('chat', graphics.newFontFamily({regular = graphics2d.defaultFont(), fallbacks = fallbacks}))
    ui.addFont('chat-code', graphics.newFontFamily({regular = fonts.load('mono'), fallbacks = fallbacks}))
    ui.addFont('layers', assets.font('fonts/bungee-color-regular.ttf'))
    ColorEmoji.registered = true
end

function ColorEmoji:enter()
    ColorEmoji.register()
    self.previousTheme = ui.theme()
    local role = {font = 'chat'}
    ui.setTheme(ui.addTheme({name = 'chat', fonts = {body = role, caption = role, button = role, heading = role, title = role, monospace = {font = 'chat-code'}}}, 'dark'))
    self:frame{hint = ColorEmoji.hint, code = ColorEmoji.code, focus = 'message', content = {ui.row{grow = 1, gap = 24, align = 'stretch', self:components(), self:details()}}}
    self.gui:transform('faded').opacity = 0.5
end

function ColorEmoji:exit()
    ui.setTheme(self.previousTheme)
    ColorEmoji.super.exit(self)
end

function ColorEmoji:components()
    return ui.panel{grow = 1, gap = 16, align = 'stretch',
        ui.sectionTitle{text = 'Labels, a field and buttons'},
        ui.label{text = 'Party time 🎉 with 👍🏽 from 12 players', font = 'heading'},
        ui.label{id = 'faded', text = 'Half opacity 😀 keeps the colors of the emoji 😂'},
        ui.textField{id = 'message', value = 'Ship it 🚀 👩‍💻 ⭐', placeholder = 'Message'},
        ui.row{gap = 12, ui.button{text = '🔥 Hot', variant = 'primary'}, ui.button{text = 'Like 👍'}, ui.chip{text = '🎉 Party', selected = true}},
        ui.richText{text = '[b]Hot[/b] 🔥 [color=#FF8A84]love[/color] ❤️ [size=40]⭐[/size] [wave]🎉🎉🎉[/wave]', font = 'heading'},
    }
end

function ColorEmoji:details()
    return ui.panel{grow = 1, gap = 16, align = 'stretch',
        ui.sectionTitle{text = 'Sequences, presentation and color fonts'},
        ui.richText{text = 'Joined: 👨 👩 👧 make 👨‍👩‍👧, 👩 💻 make 👩‍💻, 👍 with a tone 👍🏽', font = 'heading'},
        ui.richText{text = 'Text by default ☀ ❤, with U+FE0F ☀️ ❤️, emoji by default 👍 ⭐, with U+FE0E 👍︎ ⭐︎', font = 'heading'},
        ui.richText{text = '[font=layers][size=56]COLOR LAYERS[/size][/font]'},
        ui.label{text = 'Every emoji is one color glyph of a COLR font, painted with its palette, gradients and transforms at the size it covers on the screen. The display font paints the two layers of each letter with its palette.', color = 'textMuted'},
    }
end

return ColorEmoji
