-- Image layers that repeat across the view: a still sky, clouds that repeat on both axes and two ranges of hills, each scrolling at the parallax factor of its layer.
local haylen = require('haylen')
local assets = require('haylen.assets')
local input = require('haylen.input')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local Pan = require('pan')
local sample = require('sample')

local ImageLayers = haylen.class('ImageLayers', sample.Test)

local kScrollSpeed = 160

function ImageLayers:enter()
    self.map = tiled.newMapRenderer(assets.load('maps/parallax.tmj'))
    local bounds = self.map.pixelBounds
    ImageLayers.super.enter(self, {
        hint = 'The camera travels along the map. Drag it, or use the arrows or the left stick, to see the far layers move less than the near ones.',
        controls = {
            ui.toggle{id = 'travel', text = 'Travel', checked = true, onChange = function(event) self.traveling = event.checked end},
            ui.checkbox{id = 'clouds', text = 'Clouds', checked = true, onChange = function(event) self.map:setLayerVisible('clouds', event.checked) end},
            ui.checkbox{id = 'hills', text = 'Hills', checked = true, onChange = function(event)
                self.map:setLayerVisible('far hills', event.checked)
                self.map:setLayerVisible('near hills', event.checked)
            end},
        },
        stats = true,
        view = {640, bounds.height},
        focus = 'travel',
    })
    self.traveling, self.direction = true, 1
    self.pan = Pan.new(self)
    self.camera:snapTo(320, bounds.height / 2)
end

function ImageLayers:exit()
    ImageLayers.super.exit(self)
    self.map = nil
end

-- The camera rides from one end of the map to the other and turns back, and the height stays on the map.
function ImageLayers:update(dt)
    ImageLayers.super.update(self, dt)
    local bounds = self.map.pixelBounds
    local half = self.camera:visibleBounds().width / 2
    if input.pressed('reset') then
        self.camera:snapTo(half, bounds.height / 2)
    end
    self.pan:update(dt)
    if self.traveling then
        self.camera.x = self.camera.x + self.direction * kScrollSpeed * dt
        if self.camera.x > bounds.width - half or self.camera.x < half then
            self.direction = self.camera.x < half and 1 or -1
        end
    end
    self.camera.y = bounds.height / 2
    local lines = {string.format('Camera x %.0f', self.camera.x)}
    for _, layer in ipairs(self.map:layers()) do
        local repeats = layer.kind == 'image' and string.format(', repeat %s%s', layer.repeatX and 'x' or '', layer.repeatY and 'y' or '') or ''
        lines[#lines + 1] = string.format('Layer "%s": parallax %.2f%s', layer.name, layer.parallaxX, repeats)
    end
    self:showStats(table.concat(lines, '\n'))
end

function ImageLayers:render()
    self:beginWorld()
    self.map:draw(self.camera)
end

return ImageLayers
