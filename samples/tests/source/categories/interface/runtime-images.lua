-- Images made at run time in the UI: a minimap that a camera renders into a render target every frame, a portrait composed from images on the CPU and a badge built as a vector image in code, each named with "ui.setImage" and shown by images, icons and list items like the files of the package.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local RuntimeImages = haylen.class('RuntimeImages', Test)

RuntimeImages.code = [[
local minimap = graphics.newRenderTarget(240, 160)
graphics2d.beginTarget(minimap, camera, {clear = '#FF1B5E20'})  -- Every frame, in render.
local portrait = graphics.newImage(96, 96, '#FF283593')  portrait:fill('#FFFFCC80', {32, 20, 32, 32})
ui.setImage('@minimap', minimap)  ui.setImage('@portrait', portrait)  ui.setImage('@badge', graphics.newVectorImage(svg))
ui.image{image = '@minimap', width = 360, height = 240, radius = 12}  ui.button{text = 'With the badge', icon = '@badge'}]]

RuntimeImages.badge = [[<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48"><circle cx="24" cy="24" r="22" fill="#FFB300"/><path d="M24 10 L28 20 L39 20 L30 27 L33 38 L24 31 L15 38 L18 27 L9 20 L20 20 Z" fill="#FFFFFF"/></svg>]]

function RuntimeImages:enter()
    self.time = 0
    self.minimap = graphics.newRenderTarget(240, 160)
    self.minimapCamera = graphics2d.newCamera()
    ui.setImage('@minimap', self.minimap)

    local portrait = graphics.newImage(96, 96, '#FF283593')
    portrait:fill('#FF5C6BC0', {8, 8, 80, 80})
    portrait:fill('#FFFFCC80', {32, 20, 32, 32})
    portrait:fill('#FF8D6E63', {24, 56, 48, 32})
    ui.setImage('@portrait', portrait)
    ui.setImage('@badge', graphics.newVectorImage(RuntimeImages.badge))

    self:frame{
        hint = 'The minimap renders every frame into a render target that the image component shows, and the portrait and the badge were made in code.',
        code = RuntimeImages.code,
        content = {layout.columns{
            ui.column{grow = 1, gap = 24,
                layout.section('A render target', {ui.image{id = 'minimap', image = '@minimap', width = 360, height = 240, radius = 12}}),
                layout.section('An image composed in code', {ui.row{gap = 16, ui.avatar{image = '@portrait', size = 96}, ui.image{image = '@portrait', width = 96, height = 96}}}),
            },
            ui.column{grow = 1, gap = 24,
                layout.section('A vector image built in code', {ui.row{gap = 16, ui.icon{image = '@badge', size = 48}, ui.button{text = 'With the badge', icon = '@badge'}}}),
                layout.section('List items', {ui.list{id = 'list', items = {
                    {id = 'map', text = 'Minimap', caption = 'Rendered every frame', image = '@minimap', iconColor = false},
                    {id = 'portrait', text = 'Portrait', caption = 'Composed on the CPU', image = '@portrait', iconColor = false},
                    {id = 'badge', text = 'Badge', caption = 'A vector image', image = '@badge', iconColor = false},
                }}}),
            },
        }},
    }
end

function RuntimeImages:exit()
    ui.removeImage('@minimap')
    ui.removeImage('@portrait')
    ui.removeImage('@badge')
end

function RuntimeImages:update(dt)
    RuntimeImages.super.update(self, dt)
    self.time = self.time + dt
end

function RuntimeImages:render()
    RuntimeImages.super.render(self)
    graphics2d.beginTarget(self.minimap, self.minimapCamera, {clear = '#FF1B5E20'})
    for index = 0, 5 do
        local angle = self.time * 0.8 + index * math.pi / 3
        graphics2d.drawCircle(math.cos(angle) * 60, math.sin(angle) * 40, 8, index % 2 == 0 and '#FFFFEB3B' or '#FFE53935')
    end
    graphics2d.drawRectOutline({-110, -70, 220, 140}, 4, '#FFFFFFFF')
end

return RuntimeImages
