-- Every variant of button, icons above labels and content at the start, image buttons with hover and pressed pictures, chips that toggle or remove themselves and a menu button.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Buttons = haylen.class('Buttons', Test)

Buttons.code = [[
ui.button{text = 'Primary', variant = 'primary', onClick = function(event) log.info('Pressed.') end}
ui.button{text = 'Forge', icon = 'interface/icons/hammer.png', iconPlacement = 'top', variant = 'toolbar'}
ui.imageButton{image = 'interface/images/sign.png', hoverImage = 'interface/images/sign-hover.png', pressedImage = 'interface/images/sign-pressed.png', text = 'Start'}
ui.chip{id = 'coast', text = 'Coast', removable = true, onRemove = function(event) event.gui:set('coast', {visible = false}) end}
ui.menuButton{text = 'File', items = {{id = 'save', text = 'Save'}, {id = 'load', text = 'Load'}}, onSelect = function(event) log.info(event.item) end}]]

function Buttons:enter()
    self:frame{
        hint = 'Click, tap or press Enter, Space or the south button on a focused button. The hammer toolbar button stays checked, and the status line names the last press.',
        code = Buttons.code,
        focus = 'default',
        content = {self:columns()},
    }
    self:report('No button was pressed yet.')
end

function Buttons:report(text)
    self:set('status', {text = text})
end

function Buttons:pressed(name)
    return function()
        self:report('Pressed ' .. name .. '.')
    end
end

function Buttons:chip(name)
    return function(event)
        self:report(string.format('The chip "%s" is %s.', name, event.selected and 'selected' or 'not selected'))
    end
end

function Buttons:columns()
    return layout.columns{
        ui.column{grow = 1, gap = 24,
            layout.section('Button variants', {
                ui.row{wrap = true, gap = 16,
                    ui.button{id = 'default', text = 'Default', onClick = self:pressed('"default"')},
                    ui.button{text = 'Primary', variant = 'primary', onClick = self:pressed('"primary"')},
                    ui.button{text = 'Destructive', variant = 'destructive', onClick = self:pressed('"destructive"')},
                },
                ui.row{wrap = true, gap = 16,
                    ui.button{text = 'With icon', icon = 'interface/icons/sword.png', onClick = self:pressed('the button with an icon')},
                    ui.button{text = 'Link', variant = 'link', onClick = self:pressed('"link"')},
                    ui.button{text = 'Disabled', enabled = false},
                },
                ui.row{wrap = true, gap = 8,
                    ui.button{id = 'hammer', icon = 'interface/icons/hammer.png', variant = 'toolbar', checked = true, tooltip = 'Build', onClick = function(event)
                        local checked = not event.gui:get('hammer').checked
                        event.gui:set('hammer', {checked = checked})
                        self:report('The hammer is ' .. (checked and 'checked.' or 'unchecked.'))
                    end},
                    ui.button{icon = 'interface/themes/fantasy/icons/gear.png', text = 'Toolbar', variant = 'toolbar', onClick = self:pressed('"toolbar"')},
                    ui.button{id = 'favorite', icon = 'interface/themes/fantasy/icons/star.png', variant = 'icon', tooltip = 'Favorite', onClick = self:pressed('the icon button')},
                    ui.button{icon = 'interface/themes/fantasy/icons/heart.png', variant = 'icon', tooltip = 'Like', onClick = self:pressed('the heart icon button')},
                },
            }),
            layout.section('Icon placement and justify', {
                ui.row{wrap = true, gap = 0,
                    ui.button{text = 'Island', icon = 'interface/themes/fantasy/icons/star.png', iconPlacement = 'top', variant = 'toolbar', grow = 1, onClick = self:pressed('the "Island" tab')},
                    ui.button{text = 'Forge', icon = 'interface/icons/hammer.png', iconPlacement = 'top', variant = 'toolbar', grow = 1, onClick = self:pressed('the "Forge" tab')},
                    ui.button{text = 'Settings', icon = 'interface/themes/fantasy/icons/gear.png', iconPlacement = 'top', variant = 'toolbar', grow = 1, onClick = self:pressed('the "Settings" tab')},
                },
                ui.button{text = 'Search the island', icon = 'interface/icons/search.svg', justify = 'start', align = 'stretch', onClick = self:pressed('the search bar')},
            }),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('Component "imageButton"', {
                ui.row{gap = 24,
                    ui.imageButton{image = 'interface/images/sign.png', hoverImage = 'interface/images/sign-hover.png', pressedImage = 'interface/images/sign-pressed.png', scale = 0.5, text = 'Start', onClick = self:pressed('the sign')},
                    ui.imageButton{image = 'interface/images/sign.png', scale = 0.75, text = 'Big', onClick = self:pressed('the big sign')},
                    ui.imageButton{image = 'interface/themes/fantasy/icons/potion.png', scale = 0.75, tint = '#FFB0FFB0', tooltip = 'Tinted, without hover or pressed pictures', onClick = self:pressed('the potion')},
                },
            }),
            layout.section('Component "chip"', {
                ui.row{gap = 12,
                    ui.chip{text = 'Wood', selected = true, onChange = self:chip('Wood')},
                    ui.chip{text = 'Stone', onChange = self:chip('Stone')},
                    ui.chip{text = 'Fish', onChange = self:chip('Fish')},
                },
                ui.row{gap = 12,
                    ui.chip{id = 'tag-north', text = 'North', removable = true, onRemove = function(event) event.gui:set('tag-north', {visible = false}) end},
                    ui.chip{id = 'tag-coast', text = 'Coast', removable = true, onRemove = function(event) event.gui:set('tag-coast', {visible = false}) end},
                    ui.button{text = 'Bring the tags back', variant = 'link', onClick = function(event)
                        event.gui:set('tag-north', {visible = true})
                        event.gui:set('tag-coast', {visible = true})
                    end},
                },
            }),
            layout.section('Component "menuButton"', {
                ui.menuButton{id = 'file', text = 'File', icon = 'interface/icons/save.svg', items = {{id = 'save', text = 'Save'}, {id = 'load', text = 'Load'}, {id = 'cloud', text = 'Cloud sync', enabled = false}}, onSelect = function(event)
                    self:report('The menu picked "' .. event.item .. '".')
                end},
            }),
        },
    }
end

return Buttons
