-- Fonts per language: every text component draws with the family of its theme font role, whose fallback font fills in every letter the default font lacks, so labels, buttons and rich text show Japanese without changing the theme.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local language = require('language')
local sample = require('sample')

local Fonts = haylen.class('Fonts', sample.Test)

Fonts.hints = 'Pick Japanese to see labels, buttons and rich text draw it through the fallback of one family, next to the same line without the fallback.'

local kMixed = '[b]English[/b] Hello [b]Português[/b] Olá [b]Español[/b] ¡Hola! [b]日本語[/b] こんにちは'

function Fonts:content()
    local character = utf8.codepoint('語')
    local resolved = language.family:resolve(character)
    return {
        ui.panel{grow = 1, align = 'stretch', gap = 16,
            ui.sectionTitle{text = 'Labels and buttons with the family'},
            ui.label{id = 'theme', text = '', font = 'monospace', color = 'accentText'},
            ui.label{text = {key = 'fonts.sample'}, font = 'title'},
            ui.label{text = {key = 'story.chapters.first.text'}},
            ui.row{gap = 12,
                ui.button{id = 'play', text = {key = 'menu.play'}, variant = 'primary'},
                ui.button{id = 'settings', text = {key = 'menu.settings'}},
            },
            ui.label{text = 'Every text component draws with the family of its theme role, so the letters the default font lacks come from M PLUS 1p, its fallback, at the size of the em square of the default font, in every language and with one theme.', color = 'textMuted'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 16,
            ui.sectionTitle{text = 'One family with a fallback'},
            ui.label{text = 'Rich text with the family of the theme', font = 'caption', color = 'textMuted'},
            ui.richText{text = kMixed, font = 'heading'},
            ui.label{text = 'The same line with [font=default], which has no fallback', font = 'caption', color = 'textMuted'},
            ui.richText{text = '[font=default]' .. kMixed .. '[/font]', font = 'heading'},
            ui.label{text = string.format('For the code point %X, family.regular:hasGlyph is %s and the font family:resolve returns has it: %s', character, tostring(language.family.regular:hasGlyph(character)), tostring(resolved:hasGlyph(character))), font = 'monospace', color = 'accentText'},
            ui.label{text = 'The family has the default font as its regular face and M PLUS 1p as its fallback, and ui.addFont registers it for the theme roles.', color = 'textMuted'},
        },
    }
end

function Fonts:languageChanged()
    self:show('theme', {text = string.format("Language %s, theme '%s', font '%s'", localization.language(), ui.theme(), ui.themeFont('body').font)})
end

return Fonts
