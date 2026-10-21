-- Touch controls: a touch stick and two touch buttons of `haylen.ui` that write the virtual stick `move` and the virtual buttons `jump` and `dash`, which the action map reads next to every other binding, with their options changed live.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Journal = require('harness.journal')
local controls = require('categories.input.controls')
local Test = require('harness.test')

local Virtual = haylen.class('Virtual', Test)

Virtual.code = [[
local move = {name = 'move', type = 'vector', bindings = {'stick:left', 'virtualStick:move'}}
local jump = {name = 'jump', type = 'button', bindings = {'key:space', 'virtual:jump'}}
input.loadActions({actions = {move, jump}})
ui.touchStick{action = 'move', mode = 'floating', radius = 110, deadZone = 0.15}
ui.touchButton{action = 'jump', text = 'Jump', size = 130, touchOnly = true}
local mx, my = input.vector('move')]]

local kSpeed = 480
local kShotSpeed = 900
local kShipSize = 150
local kStars = 90

function Virtual:enter()
    self.shots = {}
    self.boost = 0
    self.journal = Journal(12)
    self.ship = assets.texture('physics/ship.png', {filter = 'linear'})
    self.orbs = assets.texture('sprites/images/bullets.png', {filter = 'linear'})
    self.planet = assets.texture('physics/planet-gas.png', {filter = 'linear'})
    self.time = 0
    -- The stars keep their places, picked once from a small generator of their own.
    self.stars = {}
    local seed = 23
    for index = 1, kStars do
        seed = (seed * 1103515245 + 12345) % 2147483648
        local x = seed / 2147483648
        seed = (seed * 1103515245 + 12345) % 2147483648
        self.stars[index] = {x = x, y = seed / 2147483648, size = 1.2 + index % 3 * 0.8, phase = index * 1.3}
    end
    local function logged(text, color)
        return function() self.journal:add(text .. '.', color) end
    end
    self:frame({
        code = Virtual.code,
        hint = 'Drag the stick and press the buttons with fingers, or with the mouse when no finger is down. The keys and gamepads drive the same actions.',
        play = true,
        controls = {
            ui.formField{label = 'Stick mode', ui.segmentedControl{id = 'mode', selected = 'floating', items = {{id = 'fixed', text = 'Fixed'}, {id = 'floating', text = 'Floating'}, {id = 'following', text = 'Following'}}, onChange = function(event) self:set('stick', {mode = event.value}) end}},
            ui.formField{label = 'Stick radius', ui.slider{id = 'radius', min = 60, max = 200, step = 5, value = 110, showValue = true, decimals = 0, onChange = function(event) self:set('stick', {radius = event.value}) end}},
            ui.formField{label = 'Stick dead zone', ui.slider{id = 'deadZone', min = 0, max = 0.6, step = 0.05, value = 0.15, showValue = true, onChange = function(event) self:set('stick', {deadZone = event.value}) end}},
            ui.formField{label = 'Button size', ui.slider{id = 'size', min = 80, max = 200, step = 5, value = 130, showValue = true, decimals = 0, onChange = function(event)
                self:set('jump', {size = event.value})
                self:set('dash', {size = event.value})
            end}},
            ui.toggle{id = 'touchOnly', text = 'Show only after a touch', onChange = function(event)
                for _, id in ipairs({'stick', 'jump', 'dash'}) do
                    self:set(id, {touchOnly = event.checked})
                end
            end},
        },
        overlay = {
            ui.touchStick{id = 'stick', action = 'move', mode = 'floating', anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 420, height = 420},
            ui.row{anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}, gap = 32,
                ui.touchButton{id = 'dash', action = 'dash', text = 'Dash', size = 130, onPress = logged('Dash press', Test.warm), onRelease = logged('Dash release', Test.red)},
                ui.touchButton{id = 'jump', action = 'jump', text = 'Jump', size = 130, onPress = logged('Jump press', Test.warm), onRelease = logged('Jump release', Test.red)},
            },
        },
    })
    self:loadActions(controls.actions())
end

function Virtual:exit()
    Virtual.super.exit(self)
    input.clearVirtual()
end

function Virtual:resize(area)
    self.x = self.x or area.width / 2
    self.y = self.y or area.height * 0.35
end

function Virtual:update(dt)
    Virtual.super.update(self, dt)
    if not self.area then
        return
    end
    local mx, my = input.vector('move')
    if input.pressed('dash') then
        self.boost = 0.35
    end
    self.boost = math.max(0, self.boost - dt)
    local speed = self.boost > 0 and kSpeed * 2.5 or kSpeed
    self.x = math.max(kShipSize / 2, math.min(self.area.width - kShipSize / 2, self.x + mx * speed * dt))
    self.y = math.max(kShipSize / 2, math.min(self.area.height - kShipSize / 2, self.y + my * speed * dt))
    self.time = self.time + dt

    if input.pressed('jump') then
        self.shots[#self.shots + 1] = {x = self.x, y = self.y - kShipSize * 0.45}
    end
    for index = #self.shots, 1, -1 do
        local shot = self.shots[index]
        shot.y = shot.y - kShotSpeed * dt
        if shot.y < -20 then
            table.remove(self.shots, index)
        end
    end
    self:status(string.format('Move %+.2f, %+.2f, jump %s and dash %s, with %d shots in flight. The last device is "%s".', mx, my, input.down('jump') and 'down' or 'up', input.down('dash') and 'down' or 'up', #self.shots, input.lastDevice()))
end

-- Draws the ship over a field of stars, nose up, with its flame longer while it dashes, and its shots as glowing orbs.
function Virtual:draw(area)
    graphics2d.drawShape({0, 0, area.width, area.height}, {color = '#FF0E1426'})
    for _, star in ipairs(self.stars) do
        local glow = 0.5 + 0.5 * math.sin(self.time * 1.5 + star.phase)
        graphics2d.drawCircle(star.x * area.width, star.y * area.height, star.size, {1, 0.97, 0.88, 0.35 + 0.5 * glow}, {layer = 1})
    end
    local size = math.min(area.width, area.height) * 0.42
    graphics2d.draw(self.planet, area.width * 0.36, area.height * 0.26, {width = size, height = size, color = '#FFB8C0E8', layer = 1})

    local boost = self.boost > 0
    local flame = (boost and 1.8 or 1) * (0.85 + 0.15 * math.sin(self.time * 40))
    local tail = self.y + kShipSize * 0.42
    graphics2d.drawShape({self.x - 18, tail - 10, 36, 60 * flame}, {kind = 'ellipse', color = boost and '#CCFFC93C' or '#AA6FC8FF', softness = 12, blend = 'additive', layer = 2})
    graphics2d.draw(self.ship, self.x, self.y, {width = kShipSize, height = kShipSize * self.ship.height / self.ship.width, rotation = -math.pi / 2, layer = 3})
    for _, shot in ipairs(self.shots) do
        graphics2d.drawShape({shot.x - 26, shot.y - 26, 52, 52}, {kind = 'ellipse', color = '#806FDCA0', softness = 20, blend = 'additive', layer = 2})
        graphics2d.draw(self.orbs, shot.x, shot.y, {source = {192, 0, 64, 64}, width = 30, height = 30, layer = 3})
    end

    local logWidth = 360
    local log = {area.width - logWidth - 24, 24, logWidth, 300}
    graphics2d.drawShape(log, {radius = 18, color = '#FF151A2A', borderWidth = 2, borderColor = Test.line, layer = 1})
    graphics2d.drawText(nil, 'Touch control events', log[1] + 20, log[2] + 16, {size = 22, color = Test.muted, layer = 2})
    if self.journal.lines.size == 0 then
        graphics2d.drawText(nil, 'Press Dash or Jump to see their presses and releases here.', log[1] + logWidth / 2, log[2] + log[4] / 2 + 18, {size = 22, color = Test.muted, anchor = {0.5, 0.5}, maxWidth = logWidth - 48, align = 'center', layer = 2})
        return
    end
    self.journal:draw(log[1] + 4, log[2] + 52, log[4] - 64, 19, logWidth - 16)
end

return Virtual
