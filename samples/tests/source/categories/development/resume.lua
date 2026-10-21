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

-- The chart shows this many recent frames, and a frame of this many milliseconds fills its height.
Resume.history = 120
Resume.scale = 33
Resume.file = 'source/categories/development/edit/fault.lua'
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

-- A card with the frames the test counted, the state of the error as a badge, and the work of the recent frames, which stops while the error screen shows and goes on after the fix.
function Resume:draw(area)
    local width, height = math.min(area.width - 80, 900), 360
    local left, top = (area.width - width) / 2, (area.height - height) / 2
    graphics2d.drawShape({left, top, width, height}, {radius = 24, color = '#FF1F2535', borderWidth = 3, borderColor = '#FF39415A'})
    graphics2d.drawText(nil, 'Frames updated', left + 40, top + 32, {size = 22, color = Test.muted})
    graphics2d.drawText(nil, string.format('%d', self.frames), left + 40, top + 62, {size = 64, color = Test.ink})

    local badge = self.armed and 'Armed, the update raises until a fix is saved' or 'Not armed'
    local badgeWidth = graphics2d.measureText(nil, badge, {size = 22}) + 48
    local color = self.armed and Test.red or Test.green
    graphics2d.drawShape({left + width - 40 - badgeWidth, top + 40, badgeWidth, 44}, {radius = 22, color = m.color(color):withAlpha(0.18), borderWidth = 2, borderColor = color})
    graphics2d.drawText(nil, badge, left + width - 40 - badgeWidth / 2, top + 62, {size = 22, color = color, anchor = {0.5, 0.5}})

    local chart = {left + 40, top + 170, width - 80, 150}
    graphics2d.drawText(nil, 'Work of the recent frames', chart[1], chart[2] - 30, {size = 18, color = Test.muted})
    graphics2d.drawShape(chart, {radius = 12, color = '#FF181D26'})
    local history = debugging.frameHistory()
    local count = #history
    local step = chart[3] / Resume.history
    for index = math.max(1, count - Resume.history + 1), count do
        local share = math.min(1, history[index] / Resume.scale)
        local x = chart[1] + chart[3] - (count - index + 1) * step
        local barHeight = math.max(4, (chart[4] - 16) * share)
        graphics2d.drawShape({x + 1, chart[2] + chart[4] - 8 - barHeight, step - 2, barHeight}, {radius = math.min(3, (step - 2) / 2), color = Test.accent})
    end
end

return Resume
