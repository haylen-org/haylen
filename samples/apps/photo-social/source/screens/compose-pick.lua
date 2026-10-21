-- The first step of a new post: the chosen picture large and the pictures of the library to pick from, under it on a phone and beside it on a wide window.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Compose = require('compose')
local Page = require('page')
local Photos = require('photos')
local Shell = require('shell')
local Style = require('style')

local ComposePick = haylen.class('ComposePick', Page)

ComposePick.backTransition = {duration = 0.3, effect = 'push', direction = 'down', ease = 'cubicOut'}

function ComposePick:enter()
    Compose.begin()
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('New post', {
            ui.button{id = 'next', text = 'Next', variant = 'link', onClick = function()
                self:next()
            end},
        }, {close = true}),
        ui.column{id = 'body', grow = 1, gap = 0},
    })
end

function ComposePick:layoutChanged(wide)
    local space = Style.space
    local preview = ui.image{id = 'preview', image = Compose.look(), fit = 'cover', aspectRatio = 1, accessibilityLabel = 'The chosen picture'}
    local grid = ui.collection{id = 'library', grow = 1, layout = 'grid', lanes = wide and 3 or 4, cellAspect = 1, gap = space.xxs, selection = 'single', selected = {Compose.photo}, rememberFocus = true,
        types = {photo = {template = ui.stack{align = 'stretch',
            ui.image{part = 'photo', bind = {image = 'photo'}, image = Compose.look(), fit = 'cover', align = 'stretch'},
            ui.row{align = 'stretch', alignItems = 'start', padding = space.xs, ui.spacer{grow = 1}, ui.image{part = 'picked', bind = {visible = '$selected'}, image = 'images/picked.svg', width = 36, height = 36, visible = false, accessibilityHidden = true}},
        }}},
        onSelect = function(event)
            self:pick(event.item)
        end,
    }
    local body
    if wide then
        preview.grow, preview.align = 1, 'center'
        body = ui.row{grow = 1, gap = space.l, padding = space.l, align = 'stretch',
            preview,
            ui.column{width = 480, gap = space.s, ui.label{text = 'Your pictures', font = 'button', color = 'textMuted'}, grid},
        }
    else
        preview.align = 'stretch'
        body = ui.column{grow = 1, gap = 0,
            preview,
            ui.row{padding = {space.s, space.m}, ui.label{text = 'Your pictures', font = 'button', color = 'textMuted'}},
            grid,
        }
    end
    self.gui:replaceChildren('body', {body})
    local items = {}
    for index, photo in ipairs(Photos.list) do
        items[index] = {id = photo.id, type = 'photo', photo = Photos.look(photo.id, 'full')}
    end
    self.gui:collection('library'):setItems(items)
end

function ComposePick:pick(id)
    Compose.photo = id
    self.gui:set('preview', {image = Compose.look()})
end

function ComposePick:next()
    Shell.open(require('screens.compose-edit'))
end

return ComposePick
