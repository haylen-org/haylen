-- Fonts per language: the widgets of the interface draw with the regular face of their theme font, so Japanese switches to a theme whose fonts are the Japanese font, while rich text draws from the whole family, whose fallback font fills in every letter the default font lacks, even in the Latin theme.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local language = require('language')
local sample = require('sample')

local Fonts = haylen.class('Fonts', sample.Test)

Fonts.hints = 'Pick Japanese to see the interface change its font, and any other language to see the rich text keep drawing Japanese through the fallback.'

local kMixed = '[b]English[/b] Hello [b]Português[/b] Olá [b]Español[/b] ¡Hola! [b]日本語[/b] こんにちは'

function Fonts:content()
    local character = utf8.codepoint('語')
    local resolved = language.latin:resolve(character)
    return {
        ui.panel{grow = 1, align = 'stretch', gap = 16,
            ui.sectionTitle{text = 'The interface font of the language'},
            ui.label{id = 'theme', text = '', font = 'monospace', color = 'accentText'},
            ui.label{text = {key = 'fonts.sample'}, font = 'title'},
            ui.label{text = {key = 'story.chapters.first.text'}},
            ui.row{gap = 12,
                ui.button{id = 'play', text = {key = 'menu.play'}, variant = 'primary'},
                ui.button{id = 'settings', text = {key = 'menu.settings'}},
            },
            ui.label{text = 'Widgets draw with the regular face of the font of their theme role, so each language picks a theme whose font has its letters: the default font for English, Portuguese and Spanish, and M PLUS 1p for Japanese, which has Latin letters too.', color = 'textMuted'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 16,
            ui.sectionTitle{text = 'One family with a fallback'},
            ui.label{text = 'Rich text with the family of the Latin theme', font = 'caption', color = 'textMuted'},
            ui.richText{text = kMixed, font = 'heading'},
            ui.label{text = 'The same line with [font=default], which has no fallback', font = 'caption', color = 'textMuted'},
            ui.richText{text = '[font=default]' .. kMixed .. '[/font]', font = 'heading'},
            ui.label{text = string.format('For the code point %X, family.regular:hasGlyph is %s and the font family:resolve returns has it: %s', character, tostring(language.latin.regular:hasGlyph(character)), tostring(resolved:hasGlyph(character))), font = 'monospace', color = 'accentText'},
            ui.label{text = 'The Latin family has the default font as its regular face and M PLUS 1p as its fallback, and ui.addFont registers it for the theme roles.', color = 'textMuted'},
        },
    }
end

function Fonts:languageChanged()
    self:show('theme', {text = string.format("Language %s, theme '%s'", localization.language(), ui.theme())})
end

return Fonts
