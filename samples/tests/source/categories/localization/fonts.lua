-- Every text component draws with the family of its theme font role, whose fallback fonts fill in every letter the default font lacks, so labels, buttons and rich text show Japanese without changing the theme. The test opens in Japanese, next to the same line without the fallback, whose missing letters show as empty boxes.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')
local language = require('categories.localization.language')

local Fonts = haylen.class('Fonts', LanguageTest)

Fonts.code = [[
local family = graphics.newFontFamily({regular = graphics2d.defaultFont(), fallbacks = {japanese, arabic, devanagari}})
ui.addFont('text', family)
ui.setTheme(ui.addTheme({name = 'localized', fonts = {body = {font = 'text'}, title = {font = 'text'}}}, 'dark'))
ui.richText{text = '[b]日本語[/b] こんにちは', font = 'heading'}
family:resolve(utf8.codepoint('語'))]]

Fonts.hint = 'Pick any language to see labels, buttons and rich text draw it through the fallback of one family, next to the same line without the fallback.'
Fonts.mixed = '[b]English[/b] Hello [b]Português[/b] Olá [b]Español[/b] ¡Hola! [b]日本語[/b] こんにちは'

function Fonts:content()
    local character = utf8.codepoint('語')
    local resolved = language.family:resolve(character)
    return {
        ui.panel{grow = 1, align = 'stretch',
            ui.scroll{grow = 1, height = 0, ui.column{gap = 16,
                ui.sectionTitle{text = 'Labels and buttons with the family'},
                ui.label{id = 'theme', text = '', color = 'textMuted'},
                ui.label{text = {key = 'fonts.sample'}, font = 'heading'},
                ui.label{text = {key = 'story.chapters.first.text'}},
                ui.row{wrap = true, gap = 12,
                    ui.button{id = 'play', text = {key = 'menu.play'}, variant = 'primary'},
                    ui.button{id = 'settings', text = {key = 'menu.settings'}},
                },
                ui.label{text = 'The letters the default font lacks come from the font that falls back for them, at the size of the em square of the default font, in every language and with one theme.', color = 'textMuted'},
            }},
        },
        ui.panel{grow = 1, align = 'stretch',
            ui.scroll{grow = 1, height = 0, ui.column{gap = 16,
                ui.sectionTitle{text = 'One family with fallbacks'},
                ui.card{align = 'stretch', gap = 8,
                    ui.badge{text = 'With the fallbacks of the family', tone = 'success'},
                    ui.richText{text = Fonts.mixed, font = 'heading'},
                },
                ui.card{align = 'stretch', gap = 8,
                    ui.badge{text = 'Without a fallback, the Japanese letters are missing', tone = 'danger'},
                    ui.richText{text = '[font=default]' .. Fonts.mixed .. '[/font]', font = 'heading'},
                },
                ui.label{text = string.format('The default font has no "語", the code point %X, and the font that "family:resolve" returns %s.', character, resolved:hasGlyph(character) and 'has it' or 'lacks it too'), color = 'textMuted'},
            }},
        },
    }
end

function Fonts:enter()
    Fonts.super.enter(self)
    localization.setLanguage('ja')
end

function Fonts:languageChanged()
    self:set('theme', {text = string.format('The language is "%s", and the theme "%s" draws the body with the font "%s".', localization.language(), ui.theme(), ui.themeFont('body').font)})
end

return Fonts
