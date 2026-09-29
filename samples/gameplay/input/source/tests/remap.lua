-- Remapping: a key capture for the keyboard or mouse binding and one for the gamepad binding of every gameplay action, which defines the action again and keeps the whole map in the preferences, so main.lua applies it on the next launch.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

local controls = require('controls')
local sample = require('sample')

local Remap = haylen.class('Remap', sample.Test)

local kGroups = {key = 'keyboard', mouse = 'keyboard', button = 'gamepad', axis = 'gamepad'}
local kSources = {keyboard = {'key', 'mouse'}, gamepad = {'button', 'axis'}}
local kSpeed = 420

-- Returns the first binding of the list that belongs to the group, the keyboard and mouse or the gamepad.
local function bindingOf(entry, group)
    local definition = input.actionDefinition(entry.action)
    for _, binding in ipairs(definition[entry.list] or {}) do
        if kGroups[binding:match('^(%w+):')] == group then
            return binding
        end
    end
    return ''
end

function Remap:enter()
    self.x, self.y, self.hop = nil, nil, 0
    local rows = {ui.row{gap = 16, ui.spacer{width = 200}, ui.label{text = 'Keyboard or mouse', width = 280, color = 'textMuted'}, ui.label{text = 'Gamepad', width = 280, color = 'textMuted'}}}
    for index, entry in ipairs(controls.remappable) do
        local row = {gap = 16, ui.label{text = entry.title, width = 200}}
        for _, group in ipairs({'keyboard', 'gamepad'}) do
            row[#row + 1] = ui.keyCapture{id = group .. index, value = bindingOf(entry, group), placeholder = 'None', prompt = 'Press a ' .. (group == 'keyboard' and 'key or button' or 'gamepad control'), sources = kSources[group], cancelWith = group == 'gamepad' and {'key:escape', 'button:start'} or nil, width = 280, onChange = function(event)
                self:rebind(entry, group, event.value)
            end}
        end
        rows[#rows + 1] = ui.row(row)
    end
    rows[#rows + 1] = ui.row{gap = 16,
        ui.button{id = 'reset', text = 'Reset to the defaults', onClick = function() self:reset() end},
        ui.label{id = 'saved', text = self:savedText(), color = 'textMuted', grow = 1},
    }
    rows[#rows + 1] = ui.label{text = 'Press a field, then the key, mouse button or gamepad control to bind. Escape stops listening, and so does Start for a gamepad field. Each field replaces every binding of its device in that list, and the stick and touch bindings stay.', color = 'textMuted', font = 'caption'}
    self:frame({hint = 'Rebind the controls and move the hero with them.', focus = 'keyboard1', panelWidth = 860, controls = rows})
end

function Remap:savedText()
    return preferences.has('input.actions') and 'Saved in preferences.json under input.actions' or 'Using the defaults, nothing saved yet'
end

-- Replaces every binding of the device group in the list with the new one, keeping the bindings of other devices, sticks and touch controls.
function Remap:rebind(entry, group, binding)
    local definition = input.actionDefinition(entry.action)
    local list = {binding}
    for _, current in ipairs(definition[entry.list] or {}) do
        if kGroups[current:match('^(%w+):')] ~= group then
            list[#list + 1] = current
        end
    end
    definition[entry.list] = list
    input.defineAction(definition)
    self:save()
end

function Remap:reset()
    input.loadActions(controls.defaults())
    for index, entry in ipairs(controls.remappable) do
        self:set('keyboard' .. index, {value = bindingOf(entry, 'keyboard')})
        self:set('gamepad' .. index, {value = bindingOf(entry, 'gamepad')})
    end
    self:save()
end

function Remap:save()
    preferences.capture()
    preferences.save()
    self:set('saved', {text = self:savedText()})
end

function Remap:resize(area)
    self.x = self.x or area.width / 2
    self.y = self.y or area.height / 2
end

function Remap:update(dt)
    Remap.super.update(self, dt)
    if not self.area then
        return
    end
    local mx, my = input.vector('move')
    local speed = input.down('dash') and kSpeed * 2.2 or kSpeed
    self.x = math.max(40, math.min(self.area.width - 40, self.x + mx * speed * dt))
    self.y = math.max(40, math.min(self.area.height - 40, self.y + my * speed * dt))
    self.hop = input.pressed('jump') and 0.4 or math.max(0, self.hop - dt)
    self:status(string.format('move %+.2f, %+.2f   jump %s   dash %s   stored %s', mx, my, input.down('jump'), input.down('dash'), preferences.has('input.actions')))
end

function Remap:draw(area)
    local lift = math.sin(self.hop / 0.4 * math.pi) * 60
    graphics2d.drawCircle(self.x, self.y + 30, 30, '#40000000')
    graphics2d.drawCircle(self.x, self.y - lift, 34, input.down('dash') and sample.warm or sample.accent, {layer = 1})
    sample.caption('Move, jump and dash with your bindings', area.width / 2, 24, {anchor = {0.5, 0}})

    local lines = {}
    for _, entry in ipairs(controls.remappable) do
        lines[#lines + 1] = entry.title .. ': ' .. table.concat(input.actionDefinition(entry.action)[entry.list] or {}, ', ')
    end
    graphics2d.drawText(nil, table.concat(lines, '\n'), 24, area.height - 24, {size = 17, color = sample.muted, anchor = {0, 1}, maxWidth = area.width - 48, layer = 2})
end

return Remap
