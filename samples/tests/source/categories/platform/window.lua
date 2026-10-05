-- What the engine reports about the platform, the graphics backend, the window, the visible and safe areas, the orientation and the pointer, with fullscreen, the on-screen keyboard and a simulated safe area to change, and the window, keyboard and network events as they happen.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Journal = require('harness.journal')
local Test = require('harness.test')

local Window = haylen.class('Window', Test)

Window.events = {
    windowResized = Test.muted, windowFocusGained = Test.accent, windowFocusLost = Test.accent, windowFullscreenChanged = Test.warm,
    windowOrientationChanged = Test.warm, windowSafeAreaChanged = Test.violet, keyboardShown = Test.green, keyboardHidden = Test.green,
    networkOnline = Test.green, networkOffline = Test.red, appActive = Test.accent, appInactive = Test.accent, appBackground = Test.accent,
}
Window.simulations = {
    {id = 'none', text = 'The device'}, {id = 'iphoneNotch', text = 'iPhone notch'}, {id = 'iphoneDynamicIsland', text = 'iPhone dynamic island'},
    {id = 'ipad', text = 'iPad'}, {id = 'androidGestureBar', text = 'Android gesture bar'}, {id = 'television', text = 'Television'},
}

local function rectText(rect)
    return string.format('%.0f, %.0f, %.0f x %.0f', rect.x, rect.y, rect.width, rect.height)
end

-- Writes a value that the engine reports, such as an orientation or a flag, in double quotes.
local function quoted(value)
    return '"' .. tostring(value) .. '"'
end

function Window:enter()
    self.simulation = viewport.safeAreaSimulation()
    self.safeAreaVisible = ui.safeAreaVisible()
    self.fullscreen = window.fullscreen()
    self.network = 'Online until the platform says otherwise'
    self.journal = Journal(22)
    for name, color in pairs(Window.events) do
        self:listen(name, function(value)
            self:record(name, value, color)
        end)
    end
    self:frame{
        hint = 'Resize the window, turn the device, open the keyboard or change the network.',
        focus = 'fullscreen',
        controls = {
            ui.toggle{id = 'fullscreen', text = 'Fullscreen', checked = self.fullscreen, onChange = function(event) window.setFullscreen(event.checked) end},
            ui.button{id = 'showKeyboard', text = 'Show the on-screen keyboard', onClick = function() window.setKeyboardVisible(true) end},
            ui.button{id = 'hideKeyboard', text = 'Hide the on-screen keyboard', onClick = function() window.setKeyboardVisible(false) end},
            ui.formField{label = 'A text field opens the keyboard too', ui.textField{id = 'typing', placeholder = 'Type here'}},
            ui.formField{label = 'Safe area', ui.combo{id = 'simulation', items = Window.simulations, selected = 'none', onChange = function(event)
                viewport.setSafeAreaSimulation(event.value ~= 'none' and event.value or nil)
            end}},
            ui.toggle{id = 'safeArea', text = 'Show the safe area', checked = self.safeAreaVisible, onChange = function(event) ui.setSafeAreaVisible(event.checked) end},
            ui.label{text = 'Phones, tablets and browsers report the on-screen keyboard and the network, and desktop windows always count as landscape.', color = 'textMuted', font = 'caption'},
        },
    }
end

function Window:exit()
    window.setKeyboardVisible(false)
    window.setFullscreen(self.fullscreen)
    viewport.setSafeAreaSimulation(self.simulation)
    ui.setSafeAreaVisible(self.safeAreaVisible)
    Window.super.exit(self)
end

function Window:record(name, value, color)
    local detail = ''
    if type(value) == 'table' then
        if value.width then
            detail = ', ' .. rectText(value)
        elseif value.fullscreen ~= nil then
            detail = ', fullscreen ' .. quoted(value.fullscreen)
            self:set('fullscreen', {checked = value.fullscreen})
        elseif value.orientation then
            detail = ', ' .. quoted(value.orientation)
        end
    end
    if name == 'keyboardShown' then
        self.keyboard = value
    elseif name == 'keyboardHidden' then
        self.keyboard = nil
    elseif name == 'networkOnline' or name == 'networkOffline' then
        self.network = name == 'networkOnline' and 'Online' or 'Offline'
    end
    self.journal:add('Event ' .. quoted(name) .. detail, color)
end

function Window:rows()
    local width, height = window.framebufferSize()
    local designWidth, designHeight = viewport.designSize()
    local simulation = viewport.safeAreaSimulation()
    return {
        {'"haylen.platform"', quoted(haylen.platform)},
        {'"haylen.backend"', quoted(haylen.backend)},
        {'"haylen.version"', quoted(haylen.version)},
        {'App', string.format('%s %s, %s', haylen.config.name, haylen.config.version, quoted(haylen.config.identifier))},
        {'"window.framebufferSize()"', string.format('%.0f x %.0f pixels, DPI scale %.2f', width, height, window.dpiScale())},
        {'"viewport.designSize()"', string.format('%.0f x %.0f, scaling "%s"', designWidth, designHeight, viewport.scaling())},
        {'"viewport.visibleRect()"', rectText(viewport.visibleRect())},
        {'"viewport.safeRect()"', rectText(viewport.safeRect()) .. (simulation and ', simulated ' .. quoted(simulation) or '')},
        {'"window.orientation()"', quoted(window.orientation())},
        {'"window.hasPointerDevice()"', quoted(window.hasPointerDevice())},
        {'"window.fullscreen()"', quoted(window.fullscreen())},
        {'"haylen.appState()"', quoted(haylen.appState())},
        {'Keyboard', self.keyboard and 'Shown over ' .. rectText(self.keyboard) or 'Hidden'},
        {'Network', self.network},
    }
end

function Window:update(dt)
    Window.super.update(self, dt)
    self:status(string.format('Platform "%s" on "%s", the UI wants the keyboard "%s", back leaves the app "%s"', haylen.platform, haylen.backend, tostring(ui.usingKeyboard()), tostring(window.backLeavesApp())))
end

-- Draws the visible area scaled into a box with the safe area and the keyboard inside it.
function Window:drawAreas(left, top, width)
    local visible = viewport.visibleRect()
    local scale = width / visible.width
    local height = visible.height * scale
    local safe = viewport.safeRect()
    local safeBox = {left + (safe.x - visible.x) * scale, top + (safe.y - visible.y) * scale, safe.width * scale, safe.height * scale}
    graphics2d.drawRect({left, top, width, height}, Test.surface)
    graphics2d.drawRect(safeBox, '#3070DCA0', {layer = 1})
    graphics2d.drawRectOutline(safeBox, 2, Test.green, {layer = 2})
    if self.keyboard then
        local frame = self.keyboard
        graphics2d.drawRect({left + (frame.x - visible.x) * scale, top + (frame.y - visible.y) * scale, frame.width * scale, frame.height * scale}, '#60F2B23A', {layer = 3})
    end
    Test.caption('Visible area, safe area in green, keyboard in yellow', left, top + height + 10, {size = 18, maxWidth = width})
end

function Window:draw(area)
    local y = 20
    for _, row in ipairs(self:rows()) do
        Test.caption(row[1], 24, y, {size = 20})
        Test.caption(row[2], 300, y, {size = 20, color = Test.ink})
        y = y + 30
    end
    local diagram = math.min(420, area.width * 0.3)
    self:drawAreas(area.width - diagram - 24, 20, diagram)
    Test.caption('Events', 24, y + 20, {size = 22, color = Test.ink})
    self.journal:draw(24, y + 56, area.height - y - 76, 19)
end

return Window
