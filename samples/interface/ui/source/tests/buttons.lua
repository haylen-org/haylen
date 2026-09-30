-- Buttons: every variant of button, image buttons with hover and pressed pictures, chips that toggle or remove themselves and a menu button.
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local Buttons = haylen.class('Buttons', sample.Test)

Buttons.hints = 'Click, tap or press Enter, Space or the south button on a focused button. The hammer toolbar button stays checked, and the status line names the last press.'
Buttons.focus = 'default'

function Buttons:pressed(name)
    return function()
        self:setStatus('Pressed ' .. name)
    end
end

function Buttons:content()
    return sample.columns{
        ui.column{grow = 1, gap = 24,
            sample.section('button variants', {
                ui.row{gap = 16,
                    ui.button{id = 'default', text = 'Default', onClick = self:pressed('"default"')},
                    ui.button{text = 'Primary', variant = 'primary', onClick = self:pressed('"primary"')},
                    ui.button{text = 'Destructive', variant = 'destructive', onClick = self:pressed('"destructive"')},
                },
                ui.row{gap = 16,
                    ui.button{text = 'With icon', icon = 'icons/sword.png', onClick = self:pressed('the button with an icon')},
                    ui.button{text = 'Link', variant = 'link', onClick = self:pressed('"link"')},
                    ui.button{text = 'Disabled', enabled = false},
                },
                ui.row{gap = 8,
                    ui.button{id = 'hammer', icon = 'icons/hammer.png', variant = 'toolbar', checked = true, tooltip = 'Build', onClick = function(event)
                        local checked = not event.document:get('hammer').checked
                        event.document:set('hammer', {checked = checked})
                        self:setStatus('Hammer ' .. (checked and 'checked' or 'unchecked'))
                    end},
                    ui.button{icon = 'icons/gear.png', text = 'Toolbar', variant = 'toolbar', onClick = self:pressed('"toolbar"')},
                    ui.button{id = 'favorite', icon = 'icons/star.png', variant = 'icon', tooltip = 'Favorite', onClick = self:pressed('the icon button')},
                    ui.button{icon = 'icons/heart.png', variant = 'icon', tooltip = 'Like', onClick = self:pressed('the heart icon button')},
                },
            }),
            sample.section('menuButton', {
                ui.menuButton{id = 'file', text = 'File', icon = 'icons/key.png', items = {{id = 'save', text = 'Save'}, {id = 'load', text = 'Load'}, {id = 'cloud', text = 'Cloud sync', enabled = false}}, onSelect = function(event)
                    self:setStatus('The menu picked "' .. event.item .. '"')
                end},
            }),
        },
        ui.column{grow = 1, gap = 24,
            sample.section('imageButton', {
                ui.row{gap = 24,
                    ui.imageButton{image = 'images/sign.png', hoverImage = 'images/sign_hover.png', pressedImage = 'images/sign_pressed.png', text = 'Start', onClick = self:pressed('the sign')},
                    ui.imageButton{image = 'images/sign.png', scale = 1.5, text = 'Big', onClick = self:pressed('the big sign')},
                    ui.imageButton{image = 'icons/potion.png', scale = 3, tint = '#FFB0FFB0', tooltip = 'Tinted, without hover or pressed pictures', onClick = self:pressed('the potion')},
                },
            }),
            sample.section('chip', {
                ui.row{gap = 12,
                    ui.chip{text = 'Wood', selected = true, onChange = function(event) self:setStatus('wood ' .. tostring(event.selected)) end},
                    ui.chip{text = 'Stone', onChange = function(event) self:setStatus('stone ' .. tostring(event.selected)) end},
                    ui.chip{text = 'Fish', onChange = function(event) self:setStatus('fish ' .. tostring(event.selected)) end},
                },
                ui.row{gap = 12,
                    ui.chip{id = 'tag-north', text = 'North', removable = true, onRemove = function(event) event.document:set('tag-north', {visible = false}) end},
                    ui.chip{id = 'tag-coast', text = 'Coast', removable = true, onRemove = function(event) event.document:set('tag-coast', {visible = false}) end},
                    ui.button{text = 'Bring the tags back', variant = 'link', onClick = function(event)
                        event.document:set('tag-north', {visible = true})
                        event.document:set('tag-coast', {visible = true})
                    end},
                },
            }),
        },
    }
end

return Buttons
