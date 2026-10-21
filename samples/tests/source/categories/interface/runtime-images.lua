-- Images made at run time in the UI: a minimap that a camera renders into a render target every frame, a portrait composed from images on the CPU and a badge built as a vector image in code, each named with "ui.setImage" and shown by images, icons and list items like the files of the package.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local RuntimeImages = haylen.class('RuntimeImages', Test)

RuntimeImages.code = [[
local minimap = graphics.newRenderTarget(480, 320)  -- Twice the size it shows at, so it stays sharp.
graphics2d.beginTarget(minimap, camera, {clear = '#FF1E5FA0'})  -- Every frame, in render: the island, the boat and the chests.
local portrait = graphics.newImage(160, 160)  portrait:fillGradient({kind = 'radial', stops = {{0, '#FFFFE9A0'}, {1, '#FFF2902A'}}})
portrait:blit(face, 16, 32, {blend = true})  -- A face cut from its background and outlined with face:outline('#FF1E1B33', 3).
ui.setImage('@minimap', minimap)  ui.setImage('@portrait', portrait)  ui.setImage('@badge', graphics.newVectorImage(svg))]]

RuntimeImages.badge = [[<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48"><circle cx="24" cy="24" r="21" fill="#E09A1E"/><circle cx="23" cy="23" r="18" fill="#FFC93C"/><path d="M24 11 L27.6 19.6 L37 20.3 L29.8 26.3 L32.1 35.5 L24 30.5 L15.9 35.5 L18.2 26.3 L11 20.3 L20.4 19.6 Z" fill="#FFFFFF" stroke="#1E1B33" stroke-width="2" stroke-linejoin="round"/><circle cx="24" cy="24" r="21" fill="none" stroke="#1E1B33" stroke-width="3"/></svg>]]

-- The chests on the map, in units of the minimap from its middle.
RuntimeImages.chests = {{-150, -40}, {120, 70}, {30, -100}}

function RuntimeImages:enter()
    self.time = 0
    self.minimap = graphics.newRenderTarget(480, 320)
    self.minimapCamera = graphics2d.newCamera()
    self.island = assets.texture('interface/images/island-map.png', {filter = 'linear'})
    self.boat = assets.texture('interface/images/boat-top.png', {filter = 'linear'})
    self.coin = assets.texture('interface/themes/fantasy/icons/coin.png', {filter = 'linear'})
    ui.setImage('@minimap', self.minimap)
    ui.setImage('@portrait', RuntimeImages.portrait())
    ui.setImage('@badge', graphics.newVectorImage(RuntimeImages.badge))

    self:frame{
        hint = 'The minimap renders every frame into a render target that the image component shows, and the portrait and the badge were made in code.',
        code = RuntimeImages.code,
        content = {layout.columns{
            ui.column{grow = 1, gap = 24,
                layout.section('A render target', {ui.image{id = 'minimap', image = '@minimap', width = 480, height = 320, radius = 16}}),
            },
            ui.column{grow = 1, gap = 24,
                layout.section('An image composed on the CPU', {ui.row{gap = 24, ui.avatar{image = '@portrait', size = 128}, ui.image{image = '@portrait', width = 128, height = 128, radius = 16}}}),
                layout.section('A vector image built in code', {ui.row{gap = 16, ui.icon{image = '@badge', size = 56}, ui.button{text = 'With the badge', icon = '@badge'}}}),
            },
            ui.column{grow = 1, gap = 24,
                layout.section('List items', {ui.list{id = 'list', items = {
                    {id = 'map', text = 'Minimap', caption = 'Rendered every frame', image = '@minimap', iconColor = false},
                    {id = 'portrait', text = 'Portrait', caption = 'Composed on the CPU', image = '@portrait', iconColor = false},
                    {id = 'badge', text = 'Badge', caption = 'A vector image', image = '@badge', iconColor = false},
                }}}),
            },
        }},
    }
end

-- A face cut from the flat color behind it, outlined in navy and set over a warm gradient, all on the CPU.
function RuntimeImages.portrait()
    local face = assets.load('interface/images/avatar-ana.png', 'image'):clone()
    local background = face:pixel(2, 2)
    for y = 0, face.height - 1 do
        for x = 0, face.width - 1 do
            local color = face:pixel(x, y)
            local distance = math.abs(color.r - background.r) + math.abs(color.g - background.g) + math.abs(color.b - background.b)
            if distance < 0.18 then
                face:setPixel(x, y, '#00000000')
            end
        end
    end
    face:outline('#FF1E1B33', 3)
    local portrait = graphics.newImage(160, 160)
    portrait:fillGradient({kind = 'radial', center = {0.5, 0.4}, radius = 0.75, stops = {{0, '#FFFFE9A0'}, {0.6, '#FFFFC24A'}, {1, '#FFE0702A'}}})
    portrait:blit(face, 16, 32, {blend = true})
    return portrait
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

-- The minimap draws the island, the chests and the boat that sails around it, with a ring that pulses where the boat is.
function RuntimeImages:render()
    RuntimeImages.super.render(self)
    graphics2d.beginTarget(self.minimap, self.minimapCamera, {clear = '#FF1E5FA0'})
    graphics2d.draw(self.island, 0, 0, {width = 512, height = 320})
    for index, chest in ipairs(RuntimeImages.chests) do
        local bob = math.sin(self.time * 3 + index) * 3
        graphics2d.drawCircle(chest[1], chest[2] + 14, 14, '#50000000')
        graphics2d.draw(self.coin, chest[1], chest[2] + bob, {width = 34, height = 34})
    end
    local angle = self.time * 0.35
    local x, y = math.cos(angle) * 200, math.sin(angle) * 128
    local heading = math.atan(math.cos(angle) * 128, -math.sin(angle) * 200)
    local pulse = (self.time * 0.8) % 1
    graphics2d.drawRing(x, y, 22 + pulse * 26, 3, {1, 1, 1, 0.8 * (1 - pulse)})
    graphics2d.draw(self.boat, x, y, {width = 48, height = 48, rotation = heading + math.pi / 2})
end

return RuntimeImages
