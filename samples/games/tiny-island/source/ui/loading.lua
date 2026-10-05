-- The loading view of a run: while the fade into the run holds, the chosen survivor waits at the fire and a bar follows the load. It fades in from the color of the fade.
local graphics2d = require('haylen.graphics2d')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local widgets = require('ui.widgets')

local loading = {}
loading.__index = loading

local fadeTime = 0.4

function loading.new(class, scenery)
    return setmetatable({class = class, backdrop = scenery}, loading)
end

-- The GUI belongs to the view, so it goes away when the view does.
function loading:enter()
    self.backdrop:focus(self.class.id)
    self.shown = 0
    self.gui = ui.mount(ui.column{
        id = 'content',
        padding = 96,
        gap = 28,
        justify = 'end',
        widgets.caption('title', widgets.text('loading.title')),
        ui.progress{id = 'progress', width = 900, align = 'center'},
    }, {owner = self})
    local content = self.gui:transform('content')
    content.opacity = 0
    tween.to(content, fadeTime, {opacity = 1}, {owner = self})
end

function loading:update(dt, progress)
    self.shown = self.shown + dt
    self.backdrop:update(dt)
    self.gui:set('progress', {value = progress})
end

function loading:render()
    self.backdrop:draw()
    local cover = 1 - math.min(self.shown / fadeTime, 1)
    if cover > 0 then
        graphics2d.beginScreen()
        graphics2d.drawRect(graphics2d.canvasBounds(), string.format('#%02X1B1E2B', math.floor(cover * 255)))
    end
end

return loading
