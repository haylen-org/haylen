-- Nested group layers: a village on an island over the sea, where the background group of the sea and its reefs lags behind the camera, the town group tints its walls, and its upstairs group of roofs adds its own offset, opacity and a parallax that lifts the roofs above the walls.
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')
local Pan = require('categories.tiled.pan')

local Groups = haylen.class('Groups', MapTest)

Groups.code = [[
map:draw(camera)  -- Each group passes its offset, opacity, tint and parallax on to its layers.
map:setLayerVisible('upstairs', false)  -- Hides the group and its children.
for _, layer in ipairs(map:layer('town').layers) do
    print(layer.name, layer.kind, layer.offsetX, layer.parallaxX, layer.opacity)
end]]

local kSway = 160

function Groups:enter()
    self.map = MapTest.loadMap('tiled/maps/groups.tmj')
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'The camera sways so the parallax of the groups shows: the background lags and the roofs lead. Drag, or use the arrows, the directional pad or the left stick, to move it yourself, and switch the groups on and off. R or the X button centers the camera.',
        code = Groups.code,
        controls = {
            ui.checkbox{id = 'background', text = 'Background group', checked = true, onChange = function(event) self.map:setLayerVisible('background', event.checked) end},
            ui.checkbox{id = 'town', text = 'Town group', checked = true, onChange = function(event) self.map:setLayerVisible('town', event.checked) end},
            ui.checkbox{id = 'upstairs', text = 'Upstairs group inside it', checked = true, onChange = function(event) self.map:setLayerVisible('upstairs', event.checked) end},
            ui.toggle{id = 'sway', text = 'Sway the camera', checked = true, onChange = function(event) self.swaying = event.checked end},
            ui.sectionTitle{text = 'Layers'},
            ui.tree{id = 'tree', items = Groups.items(self.map:layers()), expanded = {'background', 'town', 'upstairs'}},
        },
        view = {bounds.width + kSway * 2, bounds.height + 96},
        play = true,
    }
    self.swaying, self.time = true, 0
    self.center = {bounds.width / 2, bounds.height / 2}
    self.pan = Pan(self)
    self.camera:snapTo(self.center[1], self.center[2])
end

-- The layers as items of a tree, each group over its layers, with the offset, parallax, opacity and tint each one has of its own.
function Groups.items(layers)
    local items = {}
    for _, layer in ipairs(layers) do
        local parts = {}
        if layer.offsetX ~= 0 or layer.offsetY ~= 0 then
            parts[#parts + 1] = string.format('offset %d, %d', layer.offsetX, layer.offsetY)
        end
        if layer.parallaxX ~= 1 then
            parts[#parts + 1] = string.format('parallax %.1f', layer.parallaxX)
        end
        if layer.opacity < 1 then
            parts[#parts + 1] = string.format('opacity %.2f', layer.opacity)
        end
        if layer.tint:toHex() ~= '#FFFFFFFF' then
            parts[#parts + 1] = 'tinted'
        end
        local caption = #parts > 0 and table.concat(parts, ', '):gsub('^%l', string.upper) or 'Takes what its groups give'
        items[#items + 1] = {id = layer.name, text = string.format('%s "%s"', layer.kind == 'group' and 'Group' or 'Layer', layer.name), caption = caption, children = layer.kind == 'group' and Groups.items(layer.layers) or nil}
    end
    return items
end

function Groups:update(dt)
    Groups.super.update(self, dt)
    if input.pressed('reset') then
        self.camera:snapTo(self.center[1], self.center[2])
    end
    self.pan:update(dt)
    if self.swaying and not self.pointer.down then
        self.time = self.time + dt
        self.camera.x = self.center[1] + math.sin(self.time * 0.8) * kSway
        self.camera.y = self.center[2] + math.sin(self.time * 0.5) * kSway * 0.3
    end
    self:status(string.format('The camera is at %.0f, %.0f, and the map is centered at %.0f, %.0f.', self.camera.x, self.camera.y, self.center[1], self.center[2]))
end

function Groups:draw(area)
    self.map:draw(self.camera)
end

return Groups
