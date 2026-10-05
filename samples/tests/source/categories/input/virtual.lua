-- Touch controls: a touch stick and two touch buttons of `haylen.ui` that write the virtual stick `move` and the virtual buttons `jump` and `dash`, which the action map reads next to every other binding, with their options changed live.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Journal = require('harness.journal')
local controls = require('categories.input.controls')
local Test = require('harness.test')

local Virtual = haylen.class('Virtual', Test)

local kSpeed = 480
local kShotSpeed = 900

function Virtual:enter()
    self.shots = {}
    self.boost = 0
    self.journal = Journal(12)
    local function logged(text, color)
        return function() self.journal:add(text, color) end
    end
    self:frame({
        hint = 'Drag the stick and press the buttons with fingers, or with the mouse when no finger is down. The keys and gamepads drive the same actions.',
        play = true,
        controls = {
            ui.toggle{id = 'floating', text = 'Floating stick', checked = true, onChange = function(event) self:set('stick', {floating = event.checked}) end},
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
            ui.sectionTitle{text = 'Wiring'},
            ui.label{font = 'monospace', text = "ui.touchStick{action = 'move'}\nui.touchButton{action = 'jump'}\n-- The action map binds\n-- `virtualStick:move`\n-- `virtual:jump` and `virtual:dash`."},
        },
        overlay = {
            ui.touchStick{id = 'stick', action = 'move', floating = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 420, height = 420},
            ui.row{anchor = 'bottomRight', margin = {0, 540, 110, 0}, gap = 32,
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
    self.x = math.max(30, math.min(self.area.width - 30, self.x + mx * speed * dt))
    self.y = math.max(30, math.min(self.area.height - 30, self.y + my * speed * dt))

    if input.pressed('jump') then
        self.shots[#self.shots + 1] = {x = self.x, y = self.y - 40}
    end
    for index = #self.shots, 1, -1 do
        local shot = self.shots[index]
        shot.y = shot.y - kShotSpeed * dt
        if shot.y < -20 then
            table.remove(self.shots, index)
        end
    end
    self:status(string.format('Move %+.2f, %+.2f, jump %s, dash %s, shots %d, last device "%s"', mx, my, input.down('jump') and 'down' or 'up', input.down('dash') and 'down' or 'up', #self.shots, input.lastDevice()))
end

function Virtual:draw(area)
    local color = self.boost > 0 and Test.warm or Test.accent
    graphics2d.drawPolygon({{self.x, self.y - 36}, {self.x + 28, self.y + 26}, {self.x, self.y + 12}, {self.x - 28, self.y + 26}}, color, {layer = 2})
    for _, shot in ipairs(self.shots) do
        graphics2d.drawRect({shot.x - 4, shot.y - 16, 8, 32}, Test.green, {layer = 1})
    end
    Test.caption('Touch control events', area.width - 24, 24, {anchor = {1, 0}})
    self.journal:draw(area.width - 320, 60, 400, 19)
end

return Virtual
