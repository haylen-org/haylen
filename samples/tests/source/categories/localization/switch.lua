-- Every text of the title screen is a translation such as `{key = 'menu.play'}`, which the UI resolves every time it draws, so `localization.setLanguage` changes the whole screen at once without rebuilding it. The title screen stands on a painted landscape whose clouds drift, drawn into a render target that the card shows, with the title as a logo of warm letters over a block shadow and the subtitle on a dark plate.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local localization = require('haylen.localization')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')
local Landscape = require('categories.sprites.landscape')
local language = require('categories.localization.language')

local Switch = haylen.class('Switch', LanguageTest)

Switch.code = [[
-- The app.json of the project holds "localization": {"folder": "localization/locale", "fallback": "en"}.
localization.setLanguage('pt-BR')  -- Every text given as a key changes at once.
ui.label{text = {key = 'menu.title'}, font = 'title'}
ui.button{id = 'play', text = {key = 'menu.play'}, variant = 'primary'}
localization.language()]]

Switch.hint = 'Pick a language above or in the list, or turn on the cycle to watch every text change on its own.'
Switch.cycleSeconds = 2
Switch.backdrop = '@title-backdrop'
Switch.backdropSize = {1280, 720}
-- The backdrop measures a quarter of its pixels, so the title screen fits the region it covers.
Switch.backdropScale = 0.25
Switch.ink = '#FF1E1B33'
-- The logo is a warm fill over a copy in ink that sits a few points lower, both with an outline in ink, and the subtitle sits on a plate.
Switch.logoColor = '#FFFFD45C'
Switch.logoSize = 60
Switch.logoDepth = 6
Switch.plate = '#B0101418'

function Switch:content()
    self.landscape = Landscape()
    self.backdropTarget = graphics.newRenderTarget(Switch.backdropSize[1], Switch.backdropSize[2], {filter = 'linear'})
    self.backdropCamera = graphics2d.newCamera()
    self.backdropCamera.anchor = 'topLeft'
    ui.setImage(Switch.backdrop, self.backdropTarget)

    local buttons = {}
    for index, key in ipairs({'play', 'continue', 'settings', 'quit'}) do
        buttons[index] = ui.button{id = key, text = {key = 'menu.' .. key}, variant = index == 1 and 'primary' or 'default', align = 'stretch'}
    end
    local languages = {}
    for index, tag in ipairs(language.tags) do
        languages[index] = {id = tag, text = {key = 'language.' .. tag}, caption = 'Tag "' .. tag .. '"'}
    end
    return {
        ui.stack{grow = 1, align = 'stretch',
            ui.image{image = Switch.backdrop, fit = 'cover', scale = Switch.backdropScale, radius = 16, align = 'stretch'},
            ui.column{align = 'stretch', gap = 12, justify = 'center', padding = 12,
                ui.stack{align = 'stretch', style = {fonts = {title = {size = Switch.logoSize}}},
                    ui.label{text = {key = 'menu.title'}, font = 'title', outline = Switch.ink, outlineWidth = 6, textAlign = 'center', align = 'stretch', margin = {Switch.logoDepth, 0, 0, 0}, style = {colors = {text = Switch.ink}}},
                    ui.label{text = {key = 'menu.title'}, font = 'title', outline = Switch.ink, outlineWidth = 6, textAlign = 'center', align = 'stretch', margin = {0, 0, Switch.logoDepth, 0}, style = {colors = {text = Switch.logoColor}}},
                },
                ui.panel{align = 'center', padding = {6, 20}, style = {colors = {panel = Switch.plate}},
                    ui.label{text = {key = 'menu.subtitle'}, color = 'onAccent', textAlign = 'center'},
                },
                ui.panel{width = 500, gap = 12, align = 'center', style = {colors = {panel = '#C01B1E2B'}, metrics = {panelPadding = 20}},
                    ui.column{gap = 10, align = 'stretch', children = buttons},
                    ui.label{id = 'current', text = '', color = 'textMuted', textAlign = 'center', align = 'stretch'},
                },
            },
        },
        ui.panel{width = 560, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'The languages of the app'},
            ui.scroll{grow = 1, height = 0, style = {metrics = {listRowHeight = 56}}, ui.list{id = 'languages', items = languages, selected = localization.language(), onSelect = function(event)
                localization.setLanguage(event.item)
            end}},
            ui.label{id = 'state', text = '', color = 'textMuted'},
            ui.row{gap = 16,
                ui.label{text = 'Cycle every two seconds', grow = 1},
                ui.toggle{id = 'cycle', onChange = function(event)
                    self:cycle(event.checked)
                end},
            },
        },
    }
end

function Switch:exit()
    ui.removeImage(Switch.backdrop)
    Switch.super.exit(self)
end

-- The name of the language is an argument filled with another translation, so it is set again whenever the language changes.
function Switch:languageChanged()
    local tag = localization.language()
    self:set('current', {text = {key = 'menu.current', args = {name = localization.text('language.' .. tag)}}})
    self:set('languages', {selected = tag})
    self:set('state', {text = string.format('The call "localization.language()" returns "%s", and the fallback is "%s".', tag, localization.fallback())})
end

-- The timer belongs to the scene, so it also stops when the player leaves.
function Switch:cycle(enabled)
    if enabled then
        self.timer = timer.every(Switch.cycleSeconds, function()
            language.step(1)
        end, {owner = self})
    else
        timer.cancel(self.timer)
    end
end

-- Paints the landscape of the title screen with its clouds drifting.
function Switch:render()
    Switch.super.render(self)
    local width, height = Switch.backdropSize[1], Switch.backdropSize[2]
    graphics2d.beginTarget(self.backdropTarget, self.backdropCamera, {clear = '#FF4FA8EC'})
    self.landscape:draw({width = width, height = height}, height * 0.86, 0.9, haylen.elapsed())
end

return Switch
