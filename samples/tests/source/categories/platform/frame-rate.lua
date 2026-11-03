-- Frame rate: the app aims for every refresh of the display or for a rate the control picks, and the test measures the frames it plays each second, draws a ball that swings by the real time and a strip with a mark for each frame, so a lower rate shows as wider steps, and gives back the rate of the project when it ends.
local display = require('haylen.display')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')

local FrameRate = haylen.class('FrameRate', Test)

FrameRate.rates = {{id = '0', text = 'Every refresh'}, {id = '120', text = '120'}, {id = '60', text = '60'}, {id = '30', text = '30'}, {id = '24', text = '24'}}
FrameRate.marks = 90
FrameRate.margin = 48

FrameRate.code = [[
window.setFrameRate(30)  -- A calm screen saves battery.
window.setFrameRate(0)   -- Every refresh of the display.
print(window.frameRate(), display.current().refreshRate)]]

function FrameRate:enter()
    self.previous = window.frameRate()
    self.time = 0
    self.frames = {}
    self.marks = {}
    self:frame{
        hint = 'Pick a frame rate. The ball swings by the real time, so it moves as far at any rate, in fewer and wider steps at a lower one.',
        focus = 'rate',
        controls = {
            ui.formField{label = 'Frame rate', ui.segmentedControl{id = 'rate', items = FrameRate.rates, selected = tostring(self.previous), onChange = function(event)
                window.setFrameRate(tonumber(event.value))
                self.frames = {}
            end}},
        },
    }
end

function FrameRate:exit()
    window.setFrameRate(self.previous)
end

-- Keeps the times of the frames of the last second and the position of the ball at each of the last frames.
function FrameRate:update(dt)
    FrameRate.super.update(self, dt)
    self.time = self.time + dt
    local frames = self.frames
    frames[#frames + 1] = self.time
    while frames[1] < self.time - 1 do
        table.remove(frames, 1)
    end
    table.insert(self.marks, 1, self.time)
    self.marks[FrameRate.marks + 1] = nil

    local target = window.frameRate()
    local refresh = display.current().refreshRate
    local shown = refresh > 0 and string.format('the display refreshes at %g hertz', refresh) or 'the platform does not tell the refresh of the display'
    local aim = target == 0 and 'every refresh' or string.format('%d frames per second', target)
    self:status(string.format('The app aims for %s and played %d frames in the last second, and %s.', aim, #frames, shown))
end

function FrameRate:draw(area)
    local margin = FrameRate.margin
    local width = area.width - margin * 2
    local swing = (math.sin(self.time * math.pi) + 1) / 2
    local lane = area.height * 0.35
    graphics2d.drawRect({margin, lane - 4, width, 8}, Test.line)
    graphics2d.drawCircle(margin + swing * width, lane, 36, Test.accent)

    -- Each mark is one frame, placed by the time it played, so the gaps between marks are the gaps between frames.
    local strip = area.height * 0.7
    local span = 0.75
    for _, at in ipairs(self.marks) do
        local x = margin + width - (self.time - at) / span * width
        if x >= margin then
            graphics2d.drawRect({x - 1.5, strip - 40, 3, 80}, Test.warm)
        end
    end
    Test.caption('Each mark is a frame of the last three quarters of a second.', margin, strip + 56, {color = Test.muted})
end

return FrameRate
