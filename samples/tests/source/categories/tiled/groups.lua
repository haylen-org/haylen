-- Nested group layers: the town group moves and tints its walls, its upstairs group adds its own offset, opacity, tint and a parallax that lifts the roofs, and a background group lags behind the camera.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')
local Pan = require('categories.tiled.pan')

local Groups = haylen.class('Groups', MapTest)

local kSway = 160

function Groups:enter()
    self.map = tiled.newMapRenderer(assets.load('tiled/maps/groups.tmj'))
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'The camera sways so the parallax of the groups shows: the background lags and the roofs lead. Drag, or use the arrows, the directional pad or the left stick, to move it yourself, and switch the groups on and off. R or the X button centers the camera.',
        controls = {
            ui.checkbox{id = 'background', text = 'Background group', checked = true, onChange = function(event) self.map:setLayerVisible('background', event.checked) end},
            ui.checkbox{id = 'town', text = 'Town group', checked = true, onChange = function(event) self.map:setLayerVisible('town', event.checked) end},
            ui.checkbox{id = 'upstairs', text = 'Upstairs group inside it', checked = true, onChange = function(event) self.map:setLayerVisible('upstairs', event.checked) end},
            ui.toggle{id = 'sway', text = 'Sway the camera', checked = true, onChange = function(event) self.swaying = event.checked end},
            ui.label{id = 'details', text = '', color = 'textMuted'},
        },
        view = {bounds.width + kSway * 2, bounds.height + 96},
        play = true,
    }
    self.swaying, self.time = true, 0
    self.center = {bounds.width / 2, bounds.height / 2}
    self.pan = Pan(self)
    self.camera:snapTo(self.center[1], self.center[2])
end

-- Lists every layer under its group with what the group passes on.
function Groups:describe(layers, depth, lines)
    for _, layer in ipairs(layers) do
        local tint = layer.tint:toHex()
        lines[#lines + 1] = string.format('%sLayer "%s": offset %d, %d, parallax %.1f, opacity %.2f%s', string.rep('    ', depth), layer.name, layer.offsetX, layer.offsetY, layer.parallaxX, layer.opacity, tint ~= '#FFFFFFFF' and ', tint "' .. tint .. '"' or '')
        if layer.kind == 'group' then
            self:describe(layer.layers, depth + 1, lines)
        end
    end
    return lines
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
    self:status(string.format('Camera at %.0f, %.0f', self.camera.x, self.camera.y))
    self:details(table.concat(self:describe(self.map:layers(), 0, {}), '\n'))
end

function Groups:draw(area)
    self.map:draw(self.camera)
    graphics2d.drawRectOutline(self.map.pixelBounds, 2 * graphics2d.canvasUnitSize(), '#44FFFFFF', {layer = 5})
end

return Groups
