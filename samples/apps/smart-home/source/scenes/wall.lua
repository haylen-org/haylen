-- The only screen: it runs the day of the home, keeps the texts of the panel current, paints the wall behind the cards and draws the live pictures into their targets.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local viewport = require('haylen.viewport')

local Home = require('home')
local Panel = require('panel')
local Style = require('style')
local Views = require('views')

local Wall = haylen.class('Wall', scene.Scene)

-- The frames after a change of the window in which the pictures follow the size the GUI gives them, while the cards lay out again.
Wall.layoutFrames = 3

function Wall:enter()
    Style.load()
    Views.prepare()
    self.home = Home.new()
    self.views = Views.new({house = assets.texture('images/house.png', {filter = 'linear', mipmaps = true})})
    self.panel = Panel.new(self, self.home)
    self.wall = {pivotX = 0, pivotY = 0, x = 0, y = 0, width = 0, height = 0}
    self.layoutFrames = Wall.layoutFrames
    for _, name in ipairs({'windowResized', 'windowSafeAreaChanged'}) do
        self:listen(name, function()
            self.layoutFrames = Wall.layoutFrames
        end)
    end
end

function Wall:update(dt)
    self.home:update(dt)
    self.panel:update()
    if self.layoutFrames > 0 then
        self.layoutFrames = self.layoutFrames - 1
        self.views:fit(self.panel.gui)
        local visible = viewport.visibleRect()
        local wall = self.wall
        wall.x, wall.y, wall.width, wall.height = visible.x, visible.y, visible.width, visible.height
    end
end

function Wall:render()
    self.views:draw(self.home)
    graphics2d.beginScreen()
    local wall = self.wall
    graphics2d.draw(Style.wall, wall.x, wall.y, wall)
end

return Wall
