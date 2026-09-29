-- Window and device: what the engine reports about the platform, the graphics backend, the window, the visible and safe areas, the orientation and the pointer, with fullscreen, the on-screen keyboard and a simulated safe area to change, and the window, keyboard and network events as they happen.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Journal = require('journal')
local sample = require('sample')

local Window = haylen.class('Window', sample.Test)

local kEvents = {
    window_resized = sample.muted, window_focus_gained = sample.accent, window_focus_lost = sample.accent, window_fullscreen_changed = sample.warm,
    window_orientation_changed = sample.warm, window_safe_area_changed = sample.violet, keyboard_shown = sample.green, keyboard_hidden = sample.green,
    network_online = sample.green, network_offline = sample.red, app_active = sample.accent, app_inactive = sample.accent, app_background = sample.accent,
}
local kSimulations = {
    {id = 'none', text = 'The device'}, {id = 'iphoneNotch', text = 'iPhone notch'}, {id = 'iphoneDynamicIsland', text = 'iPhone dynamic island'},
    {id = 'ipad', text = 'iPad'}, {id = 'androidGestureBar', text = 'Android gesture bar'}, {id = 'television', text = 'Television'},
}

local function rectText(rect)
    return string.format('%.0f, %.0f, %.0f x %.0f', rect.x, rect.y, rect.width, rect.height)
end

function Window:enter()
    self.simulation = viewport.safeAreaSimulation()
    self.safeAreaVisible = ui.safeAreaVisible()
    self.network = 'online until the platform says otherwise'
    self.journal = Journal(22)
    for name, color in pairs(kEvents) do
        self:listen(name, function(value)
            self:record(name, value, color)
        end)
    end
    self:frame({
        hint = 'Resize the window, turn the device, open the keyboard or change the network.',
        focus = 'fullscreen',
        controls = {
            ui.toggle{id = 'fullscreen', text = 'Fullscreen', checked = window.fullscreen(), onChange = function(event) window.setFullscreen(event.checked) end},
            ui.button{id = 'showKeyboard', text = 'Show the on-screen keyboard', onClick = function() window.showKeyboard(true) end},
            ui.button{id = 'hideKeyboard', text = 'Hide the on-screen keyboard', onClick = function() window.showKeyboard(false) end},
            ui.formField{label = 'A text field opens the keyboard too', ui.textField{id = 'typing', placeholder = 'Type here'}},
            ui.formField{label = 'Safe area', ui.combo{id = 'simulation', items = kSimulations, selected = 'none', onChange = function(event)
                viewport.setSafeAreaSimulation(event.value ~= 'none' and event.value or nil)
            end}},
            ui.toggle{id = 'safeArea', text = 'Show the safe area', checked = self.safeAreaVisible, onChange = function(event) ui.setSafeAreaVisible(event.checked) end},
            ui.label{text = 'Phones, tablets and browsers report the on-screen keyboard and the network, and desktop windows always count as landscape.', color = 'textMuted', font = 'caption'},
        },
    })
end

function Window:exit()
    window.showKeyboard(false)
    viewport.setSafeAreaSimulation(self.simulation)
    ui.setSafeAreaVisible(self.safeAreaVisible)
end

function Window:record(name, value, color)
    local detail = ''
    if type(value) == 'table' then
        if value.width then
            detail = ' ' .. rectText(value)
        elseif value.fullscreen ~= nil then
            detail = ' fullscreen ' .. tostring(value.fullscreen)
            self:set('fullscreen', {checked = value.fullscreen})
        elseif value.orientation then
            detail = ' ' .. value.orientation
        end
    end
    if name == 'keyboard_shown' then
        self.keyboard = value
    elseif name == 'keyboard_hidden' then
        self.keyboard = nil
    elseif name == 'network_online' or name == 'network_offline' then
        self.network = name == 'network_online' and 'online' or 'offline'
    end
    self.journal:add(name .. detail, color)
end

function Window:rows()
    local width, height = window.size()
    local designWidth, designHeight = viewport.designSize()
    local simulation = viewport.safeAreaSimulation()
    return {
        {'haylen.platform', haylen.platform},
        {'haylen.backend', haylen.backend},
        {'haylen.version', haylen.version},
        {'app', string.format('%s %s, %s', haylen.config.name, haylen.config.version, haylen.config.identifier)},
        {'window.size()', string.format('%.0f x %.0f pixels, dpi scale %.2f', width, height, window.dpiScale())},
        {'viewport.designSize()', string.format('%.0f x %.0f, scaling %s', designWidth, designHeight, viewport.scaling())},
        {'viewport.visibleRect()', rectText(viewport.visibleRect())},
        {'viewport.safeRect()', rectText(viewport.safeRect()) .. (simulation and ', simulated ' .. tostring(simulation) or '')},
        {'window.orientation()', window.orientation()},
        {'window.hasPointerDevice()', tostring(window.hasPointerDevice())},
        {'window.fullscreen()', tostring(window.fullscreen())},
        {'haylen.appState()', haylen.appState()},
        {'keyboard', self.keyboard and 'shown over ' .. rectText(self.keyboard) or 'hidden'},
        {'network', self.network},
    }
end

function Window:update(dt)
    Window.super.update(self, dt)
    self:status(string.format('%s on %s   ui wants the keyboard %s   back leaves the app %s', haylen.platform, haylen.backend, ui.wantsKeyboard(), window.backLeavesApp()))
end

-- Draws the visible area scaled into a box with the safe area and the keyboard inside it.
function Window:drawAreas(left, top, width)
    local visible = viewport.visibleRect()
    local scale = width / visible.width
    local height = visible.height * scale
    local safe = viewport.safeRect()
    graphics2d.drawRect({left, top, width, height}, sample.surface)
    graphics2d.drawRect({left + (safe.x - visible.x) * scale, top + (safe.y - visible.y) * scale, safe.width * scale, safe.height * scale}, '#3070DCA0', {layer = 1})
    graphics2d.drawRectOutline({left + (safe.x - visible.x) * scale, top + (safe.y - visible.y) * scale, safe.width * scale, safe.height * scale}, 2, sample.green, {layer = 2})
    if self.keyboard then
        local frame = self.keyboard
        graphics2d.drawRect({left + (frame.x - visible.x) * scale, top + (frame.y - visible.y) * scale, frame.width * scale, frame.height * scale}, '#60F2B23A', {layer = 3})
    end
    sample.caption('Visible area, safe area in green, keyboard in yellow', left, top + height + 10, {size = 18, maxWidth = width})
end

function Window:draw(area)
    local y = 20
    for _, row in ipairs(self:rows()) do
        sample.caption(row[1], 24, y, {size = 20})
        sample.caption(row[2], 300, y, {size = 20, color = sample.ink})
        y = y + 30
    end
    local diagram = math.min(420, area.width * 0.3)
    self:drawAreas(area.width - diagram - 24, 20, diagram)
    sample.caption('Events', 24, y + 20, {size = 22, color = sample.ink})
    self.journal:draw(24, y + 56, area.height - y - 76, 19)
end

return Window
