-- What the engine reports about the platform, the graphics backend, the window, the visible and safe areas, the orientation and the pointer, with fullscreen, the on-screen keyboard and a simulated safe area to change, a picture of the screen with its safe area and keyboard, and the window, keyboard and network events as they happen.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Test = require('harness.test')

local Window = haylen.class('Window', Test)

Window.code = [[
print(haylen.platform, haylen.backend, window.orientation(), window.hasPointerDevice(), haylen.appState())
local safe = viewport.safeRect()  -- The visible area minus notches, bars and rounded corners.
window.setFullscreen(true)
window.setKeyboardVisible(true)
viewport.setSafeAreaSimulation('iphoneNotch')
events.on('keyboardShown', function(frame) print(frame.y, frame.height) end, {owner = self})
events.on('networkOffline', function() showOffline() end, {owner = self})]]

Window.events = {
    'windowResized', 'windowFocusGained', 'windowFocusLost', 'windowFullscreenChanged', 'windowOrientationChanged', 'windowSafeAreaChanged',
    'keyboardShown', 'keyboardHidden', 'networkOnline', 'networkOffline', 'appActive', 'appInactive', 'appBackground',
}
Window.simulations = {
    {id = 'none', text = 'The device'}, {id = 'iphoneNotch', text = 'iPhone notch'}, {id = 'iphoneDynamicIsland', text = 'iPhone dynamic island'},
    {id = 'ipad', text = 'iPad'}, {id = 'androidGestureBar', text = 'Android gesture bar'}, {id = 'television', text = 'Television'},
}
Window.properties = {
    {'platform', 'Platform'}, {'backend', 'Graphics backend'}, {'version', 'Engine version'}, {'app', 'App'},
    {'framebuffer', 'Framebuffer'}, {'design', 'Design size'}, {'visible', 'Visible area'}, {'safe', 'Safe area'},
    {'orientation', 'Orientation'}, {'pointer', 'Pointer device'}, {'fullscreen', 'Fullscreen'}, {'state', 'App state'},
    {'keyboard', 'On-screen keyboard'}, {'network', 'Network'},
}
Window.preview = '@window-preview'
Window.previewSize = {640, 300}
Window.kept = 30

-- Writes an area in whole design units, the way the layout places things.
local function rectText(rect)
    local function whole(value)
        return math.floor(value + 0.5)
    end
    return string.format('%d, %d, %d by %d', whole(rect.x), whole(rect.y), whole(rect.width), whole(rect.height))
end

local function yesNo(value)
    return value and 'Yes' or 'No'
end

function Window:enter()
    self.simulation = viewport.safeAreaSimulation()
    self.safeAreaVisible = ui.safeAreaVisible()
    self.fullscreen = window.fullscreen()
    self.network = 'Online until the platform says otherwise'
    self.lines = {}
    self.logged = 0
    self.previewTarget = graphics.newRenderTarget(Window.previewSize[1], Window.previewSize[2], {filter = 'linear'})
    self.previewCamera = graphics2d.newCamera()
    self.previewCamera.anchor = 'topLeft'
    ui.setImage(Window.preview, self.previewTarget)
    for _, name in ipairs(Window.events) do
        self:listen(name, function(value)
            self:record(name, value)
        end)
    end

    local rows = {}
    for index, property in ipairs(Window.properties) do
        rows[index] = ui.row{gap = 16, alignItems = 'start',
            ui.label{text = property[2], color = 'textMuted', width = 300},
            ui.label{id = 'value-' .. property[1], text = '', grow = 1},
        }
    end
    self:frame{
        hint = 'Resize the window, turn the device, open the keyboard or change the network.',
        code = Window.code,
        focus = 'fullscreen',
        controls = {
            ui.toggle{id = 'fullscreen', text = 'Fullscreen', checked = self.fullscreen, onChange = function(event) window.setFullscreen(event.checked) end},
            ui.button{id = 'showKeyboard', text = 'Show the keyboard', onClick = function() window.setKeyboardVisible(true) end},
            ui.button{id = 'hideKeyboard', text = 'Hide the keyboard', onClick = function() window.setKeyboardVisible(false) end},
            ui.formField{label = 'A text field opens the keyboard too', ui.textField{id = 'typing', placeholder = 'Type here'}},
            ui.formField{label = 'Safe area', ui.combo{id = 'simulation', items = Window.simulations, selected = 'none', onChange = function(event)
                viewport.setSafeAreaSimulation(event.value ~= 'none' and event.value or nil)
            end}},
            ui.toggle{id = 'safeArea', text = 'Show the safe area', checked = self.safeAreaVisible, onChange = function(event) ui.setSafeAreaVisible(event.checked) end},
            ui.label{text = 'Phones, tablets and browsers report the on-screen keyboard and the network, and desktop windows always count as landscape.', color = 'textMuted', font = 'caption'},
        },
        content = {ui.row{grow = 1, gap = 24, align = 'stretch',
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'What the engine reports'},
                ui.scroll{grow = 1, height = 0, ui.column{gap = 10, children = rows}},
            },
            ui.column{width = 620, gap = 16, align = 'stretch',
                ui.card{align = 'stretch', gap = 12,
                    ui.row{gap = 16,
                        ui.label{text = 'The screen', font = 'heading', grow = 1},
                        ui.statusIndicator{text = 'Safe', tone = 'success'},
                        ui.statusIndicator{text = 'Unsafe', tone = 'danger'},
                        ui.statusIndicator{text = 'Keyboard', tone = 'warning'},
                    },
                    ui.image{image = Window.preview, height = 230, radius = 12, align = 'stretch'},
                },
                ui.panel{grow = 1, align = 'stretch', gap = 12,
                    ui.sectionTitle{text = 'Events'},
                    ui.row{id = 'eventsEmpty', grow = 1, gap = 16, justify = 'center',
                        ui.icon{image = 'plugins/bell.svg', size = 64, color = 'textMuted'},
                        ui.label{text = 'No event yet. Resize the window, change the safe area or open the keyboard.', color = 'textMuted', maxWidth = 420},
                    },
                    ui.scroll{id = 'eventsScroll', grow = 1, height = 0, focusable = false, visible = false, ui.list{id = 'events', items = {}}},
                },
            },
        }},
    }
    self:showValues()
end

function Window:exit()
    window.setKeyboardVisible(false)
    window.setFullscreen(self.fullscreen)
    viewport.setSafeAreaSimulation(self.simulation)
    ui.setSafeAreaVisible(self.safeAreaVisible)
    ui.removeImage(Window.preview)
    Window.super.exit(self)
end

function Window:record(name, value)
    local detail = 'At frame ' .. haylen.frameIndex() .. '.'
    if type(value) == 'table' then
        if value.width then
            detail = 'The area is ' .. rectText(value) .. '.'
        elseif value.fullscreen ~= nil then
            detail = value.fullscreen and 'The window is fullscreen.' or 'The window left fullscreen.'
            self:set('fullscreen', {checked = value.fullscreen})
        elseif value.orientation then
            detail = 'The orientation is "' .. value.orientation .. '".'
        end
    end
    if name == 'keyboardShown' then
        self.keyboard = value
    elseif name == 'keyboardHidden' then
        self.keyboard = nil
    elseif name == 'networkOnline' or name == 'networkOffline' then
        self.network = name == 'networkOnline' and 'Online' or 'Offline'
    end
    self.logged = self.logged + 1
    table.insert(self.lines, 1, {id = 'event-' .. self.logged, text = 'Event "' .. name .. '"', caption = os.date('%H:%M:%S') .. '  ' .. detail})
    self.lines[Window.kept + 1] = nil
    self:set('events', {items = self.lines})
    self:set('eventsEmpty', {visible = false})
    self:set('eventsScroll', {visible = true})
end

function Window:values()
    local width, height = window.framebufferSize()
    local designWidth, designHeight = viewport.designSize()
    local simulation = viewport.safeAreaSimulation()
    return {
        platform = haylen.platform,
        backend = haylen.backend,
        version = haylen.version,
        app = string.format('%s %s, %s', haylen.config.name, haylen.config.version, haylen.config.identifier),
        framebuffer = string.format('%.0f by %.0f pixels at a scale of %.2f', width, height, window.dpiScale()),
        design = string.format('%.0f by %.0f, scaling "%s"', designWidth, designHeight, viewport.scaling()),
        visible = rectText(viewport.visibleRect()),
        safe = rectText(viewport.safeRect()) .. (simulation and ', simulating "' .. simulation .. '"' or ''),
        orientation = window.orientation(),
        pointer = yesNo(window.hasPointerDevice()),
        fullscreen = yesNo(window.fullscreen()),
        state = haylen.appState(),
        keyboard = self.keyboard and 'Shown over ' .. rectText(self.keyboard) or 'Hidden',
        network = self.network,
    }
end

-- Sets the values that changed since the last frame.
function Window:showValues()
    self.shown = self.shown or {}
    for key, value in pairs(self:values()) do
        if self.shown[key] ~= value then
            self.shown[key] = value
            self:set('value-' .. key, {text = value})
        end
    end
end

function Window:update(dt)
    Window.super.update(self, dt)
    self:showValues()
    self:status(string.format('The UI %s the keyboard, and the back button %s.', ui.usingKeyboard() and 'wants' or 'does not want', window.backLeavesApp() and 'leaves the app' or 'stays in the app'))
end

-- Paints the visible area scaled into the picture, with what lies outside the safe area in red, the safe area in green and the keyboard in yellow.
function Window:render()
    Window.super.render(self)
    local size = Window.previewSize
    graphics2d.beginTarget(self.previewTarget, self.previewCamera, {clear = ui.themeColor('raised')})
    local visible = viewport.visibleRect()
    local scale = math.min((size[1] - 48) / visible.width, (size[2] - 48) / visible.height)
    local width, height = visible.width * scale, visible.height * scale
    local left, top = (size[1] - width) / 2, (size[2] - height) / 2
    local function place(rect)
        return {left + (rect.x - visible.x) * scale, top + (rect.y - visible.y) * scale, rect.width * scale, rect.height * scale}
    end
    graphics2d.drawShape({left - 10, top - 10, width + 20, height + 20}, {radius = 22, color = '#FF0F111A', borderWidth = 3, borderColor = Test.line})
    graphics2d.drawShape({left, top, width, height}, {radius = 12, color = '#66FF8A84'})
    local safe = place(viewport.safeRect())
    graphics2d.drawShape(safe, {radius = 10, color = '#FF1E3A33', borderWidth = 3, borderColor = Test.green})
    graphics2d.drawText(nil, 'Safe area', safe[1] + safe[3] / 2, safe[2] + safe[4] / 2, {size = 30, color = Test.green, anchor = {0.5, 0.5}, layer = 2})
    if self.keyboard then
        graphics2d.drawShape(place(self.keyboard), {radius = 8, color = '#C0F2B23A', layer = 3})
    end
end

return Window
