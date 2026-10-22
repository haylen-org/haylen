-- An error of code that runs every frame can resume: in development, saving the fix reloads the module and the app runs on from the error screen with its state, while an error of a step of the lifecycle restarts the app.
local debugging = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local DevelopmentTest = require('categories.development.development-test')
local Test = require('harness.test')
local fault = require('categories.development.edit.fault')

local Resume = haylen.class('Resume', DevelopmentTest)

Resume.code = [[
function fault.step(armed)
    if armed then
        error('Delete this line and save the file to resume the app.')
    end
end
fault.step(self.armed)  -- The update raises until a saved fix reloads the module.]]

-- The chart shows this many recent frames, on a scale of a whole number of milliseconds that the slowest of them reaches.
Resume.history = 120
Resume.scales = {1, 2, 5, 10, 20, 50, 100, 200, 500}
Resume.files = {'source/categories/development/edit/fault.lua'}
Resume.steps = {'Press "Arm the error". The update of the test calls "fault.step", which raises, and the error screen says "Save a fix to resume the app."', 'Delete the line of "fault.step" that raises, and save the file.', 'The app leaves the error screen by itself, and the counter goes on from where it stopped.'}

function Resume:init(entry)
    Resume.super.init(self, entry)
    self.armed = false
    self.frames = 0
    self.firstControl = 'arm'
end

function Resume:controls()
    return {ui.button{id = 'arm', text = 'Arm the error', variant = 'primary', onClick = function()
        self.armed = true
    end}}
end

function Resume:update(dt)
    Resume.super.update(self, dt)
    self.frames = self.frames + 1
    fault.step(self.armed)
end

-- A card over the play area with the frames the test counted, the state of the error as a badge, and a chart of the time of the recent frames on a labeled scale, which stops while the error screen shows and goes on after the fix.
function Resume:draw(area)
    local margin = 24
    local left, top, width, height = margin, margin, area.width - margin * 2, area.height - margin * 2
    graphics2d.drawShape({left, top, width, height}, {radius = 24, color = '#FF1F2535', borderWidth = 3, borderColor = '#FF39415A'})
    graphics2d.drawText(nil, 'Frames updated', left + 40, top + 32, {size = 22, color = Test.muted})
    graphics2d.drawText(nil, string.format('%d', self.frames), left + 40, top + 62, {size = 64, color = Test.ink})

    local badge = self.armed and 'Armed, the update raises until a fix is saved' or 'Not armed'
    local badgeWidth = graphics2d.measureText(nil, badge, {size = 22}) + 48
    local color = self.armed and Test.red or Test.green
    graphics2d.drawShape({left + width - 40 - badgeWidth, top + 40, badgeWidth, 44}, {radius = 22, color = m.color(color):withAlpha(0.18), borderWidth = 2, borderColor = color})
    graphics2d.drawText(nil, badge, left + width - 40 - badgeWidth / 2, top + 62, {size = 22, color = color, anchor = {0.5, 0.5}})

    local history = debugging.frameHistory()
    local first = math.max(1, #history - Resume.history + 1)
    local slowest, total = 0, 0
    for index = first, #history do
        slowest = math.max(slowest, history[index])
        total = total + history[index]
    end
    local count = #history - first + 1
    local summary = count > 0 and string.format('Average %.1f ms, slowest %.1f ms', total / count, slowest) or 'No frame yet'
    local chartTop = top + 190
    graphics2d.drawText(nil, 'Time of the recent frames', left + 40, chartTop - 34, {size = 20, color = Test.muted})
    graphics2d.drawText(nil, summary, left + width - 40, chartTop - 34, {size = 20, color = Test.ink, anchor = {1, 0}})
    self:drawChart(history, first, slowest, {left + 40, chartTop, width - 80, top + height - 40 - chartTop})
end

-- The round bars of the frames over grid lines at zero, half and the whole of the scale, with their values at the left.
function Resume:drawChart(history, first, slowest, rect)
    local scale = Resume.scales[#Resume.scales]
    for _, value in ipairs(Resume.scales) do
        if slowest <= value then
            scale = value
            break
        end
    end
    local labels = 72
    local plot = {rect[1] + labels, rect[2] + 12, rect[3] - labels, rect[4] - 24}
    graphics2d.drawShape(rect, {radius = 12, color = '#FF181D26'})
    for _, share in ipairs({0, 0.5, 1}) do
        local y = plot[2] + plot[4] * (1 - share)
        graphics2d.drawLine(plot[1], y, plot[1] + plot[3] - 12, y, 2, share == 0 and '#FF39415A' or '#FF2A3042')
        graphics2d.drawText(nil, string.format('%g ms', scale * share), plot[1] - 14, y, {size = 16, color = Test.muted, anchor = {1, 0.5}})
    end
    local step = (plot[3] - 12) / Resume.history
    local base = plot[2] + plot[4]
    for index = first, #history do
        local barHeight = math.max(2, plot[4] * math.min(1, history[index] / scale))
        local x = plot[1] + plot[3] - 12 - (#history - index + 1) * step
        graphics2d.drawShape({x + 1, base - barHeight, step - 2, barHeight}, {radius = math.min((step - 2) / 2, barHeight / 2), color = Test.accent})
    end
end

return Resume
