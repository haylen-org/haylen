-- Remapping: a key capture for the keyboard or mouse binding and one for the gamepad binding of every gameplay action, which defines the action again and keeps the actions in the preferences, so every input test loads them, also after the next launch.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local controls = require('categories.input.controls')
local prompts = require('categories.input.prompts')
local Test = require('harness.test')

local Remap = haylen.class('Remap', Test)

Remap.code = [[
ui.keyCapture{value = 'key:space', sources = {'key'}, onChange = function(event)
    local definition = input.actionDefinition('jump')
    definition.bindings = {event.value, 'button:south'}
    input.defineAction(definition)
    preferences.set('controls', input.saveActions())  -- Kept for the next launch.
end}]]

local kGroups = {key = 'keyboard', mouse = 'keyboard', button = 'gamepad', axis = 'gamepad'}
local kSources = {keyboard = {'key', 'mouse'}, gamepad = {'button', 'axis'}}
local kSpeed = 420
-- The walker sheet has cells of 256 pixels in eight columns, the idle frames in the first row and the run in the second, with the feet on this line of the cells.
local kIdle = {0, 1, 2, 3}
local kRun = {8, 9, 10, 11, 12, 13, 14, 15}
local kFeet = 248 / 256
local kHeroSize = 200

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
    self:loadActions(controls.actions())
    self.x, self.y, self.hop, self.facing, self.time = nil, nil, 0, 1, 0
    self.walker = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    local rows = {ui.row{gap = 16, ui.spacer{width = 200}, ui.label{text = 'Keyboard or mouse', width = 260, color = 'textMuted'}, ui.label{text = 'Gamepad', width = 260, color = 'textMuted'}}}
    for index, entry in ipairs(controls.remappable) do
        local row = {gap = 16, ui.label{text = entry.title, width = 200}}
        for _, group in ipairs({'keyboard', 'gamepad'}) do
            row[#row + 1] = ui.keyCapture{id = group .. index, value = bindingOf(entry, group), placeholder = 'None', prompt = 'Press a ' .. (group == 'keyboard' and 'key or button' or 'gamepad control'), sources = kSources[group], cancelWith = group == 'gamepad' and {'key:escape', 'button:start'} or nil, width = 260, onChange = function(event)
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
    self:frame({code = Remap.code, hint = 'Rebind the controls, then click the play area or press Tab and move the hero with them.', focus = 'keyboard1', panelWidth = 860, controls = rows})
end

function Remap:savedText()
    return controls.saved() and 'Saved in the preferences under "' .. controls.preference .. '".' or 'The defaults, with nothing saved yet.'
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
    controls.save()
    self:set('saved', {text = self:savedText()})
end

function Remap:reset()
    controls.forget()
    self:loadActions(controls.defaults())
    for index, entry in ipairs(controls.remappable) do
        self:set('keyboard' .. index, {value = bindingOf(entry, 'keyboard')})
        self:set('gamepad' .. index, {value = bindingOf(entry, 'gamepad')})
    end
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
    self.x = math.max(80, math.min(self.area.width - 80, self.x + mx * speed * dt))
    self.y = math.max(kHeroSize, math.min(self.area.height - 64 - #controls.remappable * 42, self.y + my * speed * dt))
    self.hop = input.pressed('jump') and 0.4 or math.max(0, self.hop - dt)
    self.moving = math.abs(mx) + math.abs(my) > 0.1
    self.time = self.time + dt * (input.down('dash') and 1.8 or 1)
    if math.abs(mx) > 0.1 then
        self.facing = mx > 0 and 1 or -1
    end
    self:status(string.format('Move %+.2f, %+.2f, jump %s and dash %s, with %s.', mx, my, input.down('jump') and 'down' or 'up', input.down('dash') and 'down' or 'up', controls.saved() and 'the stored bindings' or 'the default bindings'))
end

function Remap:draw(area)
    local lift = math.sin(self.hop / 0.4 * math.pi) * 70
    local frames = self.moving and kRun or kIdle
    local frame = frames[math.floor(self.time * (self.moving and 14 or 6)) % #frames + 1]
    local dashing = input.down('dash')
    graphics2d.drawShape({self.x - 60, self.y - 12, 120, 24}, {kind = 'ellipse', color = '#55000000', softness = 8})
    graphics2d.draw(self.walker, self.x, self.y - lift, {source = {frame % 8 * 256, frame // 8 * 256, 256, 256}, width = kHeroSize, height = kHeroSize, pivotY = kFeet, flipHorizontal = self.facing < 0, flash = dashing and '#55FFC93C' or nil, layer = 1})
    graphics2d.drawText(nil, 'Move, jump and dash with your bindings', area.width / 2, 24, {size = 24, color = Test.ink, anchor = {0.5, 0}, layer = 2})

    -- The bindings of every action, as chips of what the player presses.
    local margin, size = 24, 34
    local rows = #controls.remappable
    local top = area.height - margin - rows * (size + 8)
    graphics2d.drawShape({margin, top - 16, area.width - margin * 2, area.height - top - margin + 16}, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    for index, entry in ipairs(controls.remappable) do
        local y = top + (index - 1) * (size + 8) + size / 2
        graphics2d.drawText(nil, entry.title, margin + 20, y, {size = 22, color = Test.muted, anchor = {0, 0.5}, layer = 2})
        prompts.chips(input.actionDefinition(entry.action)[entry.list] or {}, margin + 180, y, size, area.width - margin * 2, 3)
    end
end

return Remap
