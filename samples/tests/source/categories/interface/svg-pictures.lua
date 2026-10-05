-- SVG documents as the pictures of components, rasterized for the pixels they cover so they stay sharp at every size and UI scale, buttons that tint their icons and pictures with rounded corners.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local SvgPictures = haylen.class('SvgPictures', Test)

SvgPictures.icons = {'heart', 'star', 'leaf', 'bolt', 'chat', 'gear'}
SvgPictures.sizes = {16, 24, 32, 48, 64, 96, 128}

function SvgPictures:init(entry)
    SvgPictures.super.init(self, entry)
    self.started = (ui.scale())
end

function SvgPictures.picture(name)
    return 'sprites/vector/' .. name .. '.svg'
end

function SvgPictures:enter()
    self:frame{
        hint = 'Move the UI scale to see the icons draw again for the size they cover instead of blurring. The buttons tint their icons with "iconColor", and the photo rounds its corners with "radius".',
        focus = 'scale',
        content = {self:columns()},
    }
end

function SvgPictures:exit()
    ui.setScale(self.started)
    SvgPictures.super.exit(self)
end

function SvgPictures:columns()
    local sizes = {}
    for index, size in ipairs(SvgPictures.sizes) do
        sizes[index] = ui.column{gap = 4, align = 'end', ui.icon{image = SvgPictures.picture(SvgPictures.icons[(index - 1) % #SvgPictures.icons + 1]), size = size}, ui.label{text = size .. ' units', font = 'caption', color = 'textMuted'}}
    end
    local items = {}
    for index, name in ipairs(SvgPictures.icons) do
        items[index] = {id = name, text = name:sub(1, 1):upper() .. name:sub(2), caption = 'An SVG picture in a list row', image = SvgPictures.picture(name)}
    end
    return layout.columns{
        ui.column{grow = 1, gap = 24,
            layout.section('UI scale', {ui.slider{id = 'scale', min = 0.5, max = 2, step = 0.25, value = self.started, showValue = true, onChange = function(event) ui.setScale(event.value) end}}),
            layout.section('One document at every size', {ui.row{gap = 18, wrap = true, alignItems = 'end', children = sizes}}),
            layout.section('Buttons', {ui.row{gap = 12, wrap = true,
                ui.button{id = 'plain', text = 'Own colors', icon = SvgPictures.picture('heart')},
                ui.button{id = 'tinted', text = 'Tinted', icon = SvgPictures.picture('bolt'), iconColor = 'accent'},
                ui.button{id = 'primary', text = 'Primary', icon = SvgPictures.picture('leaf'), variant = 'primary', iconColor = 'onAccent'},
                ui.button{id = 'toolbar', text = 'Toolbar', icon = SvgPictures.picture('gear'), variant = 'toolbar'},
                ui.imageButton{id = 'picture', image = SvgPictures.picture('star'), scale = 1.5},
            }}),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('Rounded corners', {ui.row{gap = 16, wrap = true,
                ui.image{image = 'interface/images/landscape_day.png', width = 220, height = 140, fit = 'cover'},
                ui.image{image = 'interface/images/landscape_day.png', width = 220, height = 140, fit = 'cover', radius = 24},
                ui.image{image = SvgPictures.picture('chat'), width = 140, height = 140, radius = 70},
                ui.avatar{image = SvgPictures.picture('heart'), size = 96},
            }}),
            layout.section('List rows', {ui.list{id = 'rows', items = items}}),
        },
    }
end

return SvgPictures
