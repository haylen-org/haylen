-- The fantasy image theme of the test project next to the default theme, at several UI scales: every component draws from one atlas of nine-slice pieces and a set of icons.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local ImageTheme = haylen.class('ImageTheme', Test)

ImageTheme.themes = {{id = 'dark', text = 'Default'}, {id = 'fantasy', text = 'Fantasy'}}
ImageTheme.scales = {{id = '0.75', text = '0.75x'}, {id = '1', text = '1x'}, {id = '1.25', text = '1.25x'}, {id = '1.5', text = '1.5x'}}
ImageTheme.backgrounds = {dark = '#FF101418', fantasy = '#FF1A1D30'}
ImageTheme.icons = {'heart', 'star', 'coin', 'gem', 'gear', 'shield', 'potion', 'key'}

function ImageTheme:enter()
    self.started = {theme = ui.theme(), factor = ui.scale()}
    ui.loadTheme('interface/themes/fantasy/theme.json', 'dark')
    self:frame{
        hint = 'Pick the theme and the UI scale. Hold a button or a switch to see it pressed, walk the controls with the arrows to see the focus ring, and open the overlays with their buttons.',
        focus = 'theme',
        content = self:nodes(),
    }
    self:describe()
end

function ImageTheme:exit()
    ui.setTheme(self.started.theme)
    ui.setScale(self.started.factor)
    ImageTheme.super.exit(self)
end

function ImageTheme:describe()
    self:set('status', {text = string.format('Theme "%s" at the scale %.2f, buttons drawn %s.', ui.theme(), ui.scale(), ui.themeSurface('button') and 'from the atlas' or 'with flat colors')})
end

function ImageTheme:report(text)
    self:set('status', {text = text})
end

function ImageTheme.icon(name)
    return 'interface/themes/fantasy/icons/' .. name .. '.png'
end

function ImageTheme:buttons()
    return layout.section('Buttons, chips and badges', {
        ui.row{gap = 12, wrap = true, ui.button{text = 'Default'}, ui.button{text = 'Primary', variant = 'primary'}, ui.button{text = 'Delete', variant = 'destructive'}},
        ui.row{gap = 12, wrap = true,
            ui.button{text = 'Shop', icon = ImageTheme.icon('coin')},
            ui.button{icon = ImageTheme.icon('gear'), variant = 'icon', tooltip = 'Settings'},
            ui.button{icon = ImageTheme.icon('shield'), variant = 'toolbar', checked = true, tooltip = 'Defend'},
            ui.button{text = 'Locked', enabled = false},
        },
        ui.row{gap = 12, wrap = true, ui.chip{text = 'Swords', selected = true}, ui.chip{text = 'Bows'}, ui.chip{text = 'Staves', removable = true}},
        ui.row{gap = 12, wrap = true,
            ui.badge{text = '3', tone = 'danger', solid = true},
            ui.badge{text = '12', tone = 'accent', solid = true},
            ui.badge{text = '99+', tone = 'warning', solid = true},
            ui.badge{text = 'New', tone = 'success', solid = true},
            ui.badge{text = 'Quest', tone = 'information'},
        },
        ui.segmentedControl{items = {{id = 'day', text = 'Day'}, {id = 'dusk', text = 'Dusk'}, {id = 'night', text = 'Night'}}, selected = 'day'},
    })
end

function ImageTheme:choices()
    return layout.section('Choices and fields', {
        ui.row{gap = 24, wrap = true, ui.checkbox{text = 'Hints', checked = true}, ui.checkbox{text = 'Subtitles'}},
        ui.radioGroup{items = {{id = 'easy', text = 'Easy'}, {id = 'normal', text = 'Normal'}, {id = 'hard', text = 'Hard'}}, selected = 'normal', horizontal = true},
        ui.row{gap = 24, wrap = true, ui.toggle{id = 'music', text = 'Music', checked = true}, ui.toggle{text = 'Sound'}, ui.toggle{text = 'Off', enabled = false}},
        ui.textField{placeholder = 'Name of the hero'},
        ui.combo{items = {{id = 'warrior', text = 'Warrior'}, {id = 'archer', text = 'Archer'}, {id = 'mage', text = 'Mage'}, {id = 'lancer', text = 'Lancer'}}, selected = 'warrior'},
        ui.stepper{items = {{id = 'small', text = 'Small island'}, {id = 'large', text = 'Large island'}}, selected = 'small'},
    })
end

function ImageTheme:bars()
    return layout.section('Bars and sliders', {
        ui.progress{value = 0.72, tone = 'success', text = 'Health'},
        ui.progress{value = 0.35, tone = 'information', text = 'Mana'},
        ui.progress{value = 0.55, text = 'Loading'},
        ui.slider{value = 0.6, showValue = true},
        ui.rangeSlider{low = 0.2, high = 0.7},
    })
end

function ImageTheme:collections()
    local rows = {}
    for index = 1, 12 do
        local name = ImageTheme.icons[(index - 1) % #ImageTheme.icons + 1]
        rows[index] = {id = 'item-' .. index, text = name:sub(1, 1):upper() .. name:sub(2) .. ' ' .. index, caption = 'Row ' .. index .. ' of the bag', image = ImageTheme.icon(name)}
    end
    return layout.section('Tabs, lists and scroll bars', {
        ui.tabs{items = {{id = 'bag', text = 'Bag'}, {id = 'quests', text = 'Quests'}, {id = 'map', text = 'Map'}}, selected = 'bag',
            ui.scroll{height = 300, ui.list{items = rows, selected = 'item-2', onSelect = function(event) self:report('The list picked "' .. event.item .. '".') end}},
            ui.label{text = 'No quests yet.', color = 'textMuted'},
            ui.label{text = 'The map is still folded.', color = 'textMuted'},
        },
    })
end

function ImageTheme:overlays()
    local icons = {}
    for index, name in ipairs(ImageTheme.icons) do
        icons[index] = ui.icon{image = ImageTheme.icon(name), size = 56, tooltip = name:sub(1, 1):upper() .. name:sub(2)}
    end
    return layout.section('Overlays and icons', {
        ui.row{gap = 12, wrap = true,
            ui.button{text = 'Dialog', onClick = function(event) event.gui:set('dialog', {open = true}) end},
            ui.button{text = 'Window', onClick = function(event) event.gui:set('window', {open = true}) end},
            ui.button{text = 'Toast', onClick = function(event) event.gui:command('toast', 'show', {text = 'The chest is open.', tone = 'success'}) end},
        },
        ui.button{text = 'Rest here for a tooltip', tooltip = 'Tooltips take the tooltip piece of the atlas.'},
        ui.row{gap = 12, wrap = true, children = icons},
    })
end

function ImageTheme:nodes()
    return {
        ui.row{gap = 24,
            ui.label{text = 'Theme', font = 'heading'},
            ui.segmentedControl{id = 'theme', width = 420, items = ImageTheme.themes, selected = 'dark', onChange = function(event)
                ui.setTheme(event.value)
                self:describe()
            end},
            ui.label{text = 'Scale', font = 'heading'},
            ui.segmentedControl{id = 'scale', width = 520, items = ImageTheme.scales, selected = '1', onChange = function(event)
                ui.setScale(tonumber(event.value))
                self:describe()
            end},
        },
        ui.scroll{grow = 1, height = 0,
            ui.grid{minColumnWidth = 560, gap = 24, self:buttons(), self:choices(), self:collections(), self:bars(), self:overlays()},
        },
        ui.dialog{id = 'dialog', title = 'Open the chest?', message = 'The key breaks once the chest opens.', buttons = {{id = 'leave', text = 'Leave it'}, {id = 'open', text = 'Open', variant = 'primary'}},
            onAnswer = function(event) self:report('The dialog answered "' .. event.button .. '".') end,
        },
        ui.window{id = 'window', title = 'Satchel', open = false, x = 1180, y = 300, width = 560, closable = true,
            ui.label{text = 'A window with a title bar from the atlas.', color = 'textMuted'},
            ui.row{gap = 12, ui.icon{image = ImageTheme.icon('potion'), size = 56}, ui.icon{image = ImageTheme.icon('key'), size = 56}, ui.button{text = 'Drink', variant = 'primary'}},
        },
        ui.toast{id = 'toast', duration = 3},
    }
end

-- The screen behind the GUIs takes the background of the theme.
function ImageTheme:render()
    graphics2d.beginScreen()
    graphics2d.drawRect(viewport.visibleRect(), ImageTheme.backgrounds[ui.theme()] or Test.stageColor)
end

return ImageTheme
