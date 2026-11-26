-- Displays: every display of the device drawn where it sits, with its id, its name, its scale, its rate and its modes, the window on the display that shows it, and the buttons that move the window to another display, cover it, cover it in its lowest mode, resize the window and ask a browser for every screen. The events of the displays mark the drawing as they arrive, and the last one reads under the buttons.
local display = require('haylen.display')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')

local Displays = haylen.class('Displays', Test)

Displays.code = "for _, shown in ipairs(display.list()) do print(shown.id, shown.bounds, shown.refreshRate) end\nwindow.place({anchor = 'center', display = other.id})\nwindow.setFullscreen(true, {display = other.id, mode = other.modes[#other.modes]})\nwindow.setFrame(0, 0, 960, 540)  -- The size alone on visionOS.\nevents.on('displayConnected', function(info) print(info.name) end)\ndisplay.requestEveryScreen():await()  -- Every screen of a browser."
Displays.margin = 48

function Displays:enter()
    self.heard = {}
    self:frame{
        hint = 'Move the window to the next display, cover it, cover it in its lowest mode or resize it. Connect or disconnect a display, or move the window by hand, to see the events.',
        focus = 'next',
        controls = {
            ui.button{id = 'next', text = 'Place on the next display', variant = 'primary', onClick = function() self:place() end},
            ui.button{id = 'cover', text = 'Cover the next display', onClick = function() self:cover(false) end},
            ui.button{id = 'exclusive', text = 'Cover it in its lowest mode', onClick = function() self:cover(true) end},
            ui.button{id = 'leave', text = 'Leave fullscreen', onClick = function() window.setFullscreen(false) end},
            ui.button{id = 'resize', text = 'Resize the window', onClick = function() self:resizeWindow() end},
            ui.button{id = 'request', text = 'Ask for every screen', onClick = function() self:request() end},
            ui.label{id = 'answer', text = '', color = 'textMuted', font = 'caption'},
            ui.label{id = 'latest', text = 'No event of the displays yet.', color = 'textMuted', font = 'caption'},
        },
    }
    if display.unlisted() then
        self:set('answer', {text = self:describeSingle()})
    end
    for _, name in ipairs({'displayConnected', 'displayDisconnected', 'displayChanged', 'windowDisplayChanged'}) do
        events.on(name, function(info)
            local line = string.format('%s %s', name, (info.name and info.name ~= '') and info.name or info.display)
            table.insert(self.heard, 1, line)
            self.heard[6] = nil
            self:set('latest', {text = 'Last event: ' .. line})
        end, {owner = self})
    end
end

-- A browser that hides the other screens until the person lets the page see them says that there are more, so the test asks for them instead of calling the device single.
function Displays:describeSingle()
    if display.unlisted() then
        return 'This browser shows the app only the screen of its window until the person lets it see every screen. Click "Ask for every screen" to see the others.'
    end
    return 'This device has a single display.'
end

-- The display after the one of the window, in the order of the list, or nil with a single display.
function Displays:nextDisplay()
    local all = display.list()
    local current = display.current().id
    for index, shown in ipairs(all) do
        if shown.id == current and #all > 1 then
            return all[index % #all + 1]
        end
    end
    return nil
end

function Displays:place()
    local other = self:nextDisplay()
    if not other then
        self:set('answer', {text = self:describeSingle()})
        return
    end
    window.place({anchor = 'center', display = other.id})
end

-- The lowest mode is the last one that covers the fewest pixels, which every display runs in.
function Displays:cover(exclusive)
    local other = self:nextDisplay() or display.current()
    local lowest = nil
    for _, mode in ipairs(other.modes) do
        if not lowest or mode.width * mode.height < lowest.width * lowest.height then
            lowest = mode
        end
    end
    local ok, answer = pcall(window.setFullscreen, true, {display = other.id, mode = exclusive and lowest or nil})
    if not ok then
        self:set('answer', {text = answer})
        return
    end
    self:spawn(function()
        local _, message = answer:await()
        self:set('answer', {text = message or string.format('Fullscreen on "%s"%s.', other.name ~= '' and other.name or other.id, exclusive and lowest and string.format(' at %d by %d', lowest.width, lowest.height) or '')})
    end)
end

-- The window shrinks and grows back in turns. A desktop moves and sizes the window, visionOS sizes the window that is its one display, and a window that fills its screen keeps its size.
function Displays:resizeWindow()
    local frame = window.frame()
    self.shrunk = not self.shrunk
    local scale = self.shrunk and 0.8 or 1.25
    window.setFrame(frame.x, frame.y, math.floor(frame.width * scale), math.floor(frame.height * scale))
end

-- A browser lists every screen once the person lets the page see them, which the request asks for.
function Displays:request()
    self:spawn(function()
        local _, message = display.requestEveryScreen():await()
        self:set('answer', {text = message or string.format('The app sees %s.', Displays.count(#display.list(), 'display'))})
    end)
end

-- A count with its noun, such as "1 mode" or "2 modes".
function Displays.count(number, noun)
    return string.format('%d %s%s', number, noun, number == 1 and '' or 's')
end

-- The refresh rate of a display, which some platforms, such as browsers, never tell.
function Displays.rate(refreshRate)
    if refreshRate == 0 then
        return 'unknown rate'
    end
    return string.format('%g Hz', refreshRate)
end

-- The displays fit the play area with their places on the desktop, or side by side on platforms whose displays have points of their own.
function Displays:draw(area)
    local all = display.list()
    local desktop = haylen.platform == 'macos' or haylen.platform == 'windows' or haylen.platform == 'linux' or haylen.platform == 'headless'
    local left, top, right, bottom = math.huge, math.huge, -math.huge, -math.huge
    local places = {}
    local offset = 0
    for index, shown in ipairs(all) do
        local bounds = shown.bounds
        local x = desktop and bounds.x or offset
        places[index] = {x = x, y = desktop and bounds.y or 0, width = bounds.width, height = bounds.height}
        offset = offset + bounds.width + 120
        left, top = math.min(left, places[index].x), math.min(top, places[index].y)
        right, bottom = math.max(right, places[index].x + bounds.width), math.max(bottom, places[index].y + bounds.height)
    end
    local room = area.height - Displays.margin * 2 - 140
    local fit = math.min((area.width - Displays.margin * 2) / math.max(right - left, 1), room / math.max(bottom - top, 1))
    local originX = (area.width - (right - left) * fit) / 2 - left * fit
    local originY = Displays.margin

    local current = display.current().id
    for index, shown in ipairs(all) do
        local place = places[index]
        local x, y = originX + place.x * fit, originY + (place.y - top) * fit
        local width, height = place.width * fit, place.height * fit
        local shows = shown.id == current
        graphics2d.drawRect({x, y, width, height}, shows and '#FF26324A' or Test.surface)
        graphics2d.drawRectOutline({x, y, width, height}, shows and 3 or 1.5, shown.primary and Test.accent or Test.line)
        -- The captions sit inside the work area, so its outline never crosses them.
        local work = shown.workArea
        local workLeft, workTop = x + (work.x - shown.bounds.x) * fit, y + (work.y - shown.bounds.y) * fit
        local workWidth, workHeight = work.width * fit, work.height * fit
        graphics2d.drawRectOutline({workLeft, workTop, workWidth, workHeight}, 1, Test.muted)
        Test.caption(shown.name ~= '' and shown.name or shown.id, workLeft + 12, workTop + 10, {size = 22, color = Test.ink, maxWidth = workWidth - 24})
        Test.caption(string.format('%g by %g points at %gx, %s, %s', shown.bounds.width, shown.bounds.height, shown.scale, Displays.rate(shown.refreshRate), #shown.modes == 0 and 'no modes' or Displays.count(#shown.modes, 'mode')), workLeft + 12, workTop + 40, {maxWidth = workWidth - 24})
        if shows then
            Test.caption('The window is here', workLeft + 12, workTop + workHeight - 34, {color = Test.green, maxWidth = workWidth - 24})
        end
    end

    for index, line in ipairs(self.heard) do
        Test.caption(line, Displays.margin, area.height - Displays.margin - index * 26 + 26, {color = index == 1 and Test.warm or Test.muted})
    end
end

function Displays:update(dt)
    Displays.super.update(self, dt)
    local current = display.current()
    self:status(string.format('%s, the window is on "%s"%s.', Displays.count(#display.list(), 'display'), current.name ~= '' and current.name or current.id, window.fullscreen() and ' in fullscreen' or ''))
end

return Displays
