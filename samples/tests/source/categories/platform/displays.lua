-- Displays: every display of the device drawn where it sits, with its id, its name, its scale, its rate and its modes, the window on the display that shows it, and the buttons that move the window to another display, cover it, cover it in its lowest mode and ask a browser for every screen. The events of the displays mark the drawing as they arrive.
local display = require('haylen.display')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')

local Displays = haylen.class('Displays', Test)

Displays.code = "for _, shown in ipairs(display.list()) do print(shown.id, shown.bounds, shown.refreshRate) end\nwindow.place({anchor = 'center', display = other.id})\nwindow.setFullscreen(true, {display = other.id, mode = other.modes[#other.modes]})\nevents.on('displayConnected', function(info) print(info.name) end)\ndisplay.request():await()  -- Every screen of a browser."
Displays.margin = 48

function Displays:enter()
    self.heard = {}
    self:frame{
        hint = 'Move the window to the next display, cover it, or cover it in its lowest mode. Connect or disconnect a display, or move the window by hand, to see the events.',
        focus = 'next',
        controls = {
            ui.button{id = 'next', text = 'Place on the next display', variant = 'primary', onClick = function() self:place() end},
            ui.button{id = 'cover', text = 'Cover the next display', onClick = function() self:cover(false) end},
            ui.button{id = 'exclusive', text = 'Cover it in its lowest mode', onClick = function() self:cover(true) end},
            ui.button{id = 'leave', text = 'Leave fullscreen', onClick = function() window.setFullscreen(false) end},
            ui.button{id = 'request', text = 'Ask for every screen', onClick = function() self:request() end},
            ui.label{id = 'answer', text = '', color = 'textMuted', font = 'caption'},
        },
    }
    for _, name in ipairs({'displayConnected', 'displayDisconnected', 'displayChanged', 'windowDisplayChanged'}) do
        events.on(name, function(info)
            table.insert(self.heard, 1, string.format('%s %s', name, info.name or info.display))
            self.heard[6] = nil
        end, {owner = self})
    end
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
        self:set('answer', {text = 'This device has a single display.'})
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
    local ok, message = pcall(window.setFullscreen, true, {display = other.id, mode = exclusive and lowest or nil})
    self:set('answer', {text = ok and string.format('Fullscreen on "%s"%s.', other.name ~= '' and other.name or other.id, exclusive and lowest and string.format(' at %d by %d', lowest.width, lowest.height) or '') or message})
end

function Displays:request()
    self:spawn(function()
        local listed, message = display.request():await()
        self:set('answer', {text = listed and string.format('The app sees %d displays.', #display.list()) or message})
    end)
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
        local work = shown.workArea
        graphics2d.drawRectOutline({x + (work.x - shown.bounds.x) * fit, y + (work.y - shown.bounds.y) * fit, work.width * fit, work.height * fit}, 1, Test.muted)
        Test.caption(shown.name ~= '' and shown.name or shown.id, x + 12, y + 10, {size = 22, color = Test.ink, maxWidth = width - 24})
        Test.caption(string.format('%g by %g points at %gx, %g Hz, %d modes', shown.bounds.width, shown.bounds.height, shown.scale, shown.refreshRate, #shown.modes), x + 12, y + 40, {maxWidth = width - 24})
        if shows then
            Test.caption('The window is here', x + 12, y + height - 34, {color = Test.green})
        end
    end

    for index, line in ipairs(self.heard) do
        Test.caption(line, Displays.margin, area.height - Displays.margin - index * 26 + 26, {color = index == 1 and Test.warm or Test.muted})
    end
end

function Displays:update(dt)
    Displays.super.update(self, dt)
    local current = display.current()
    self:status(string.format('%d displays, the window is on "%s"%s.', #display.list(), current.name ~= '' and current.name or current.id, window.fullscreen() and ' in fullscreen' or ''))
end

return Displays
