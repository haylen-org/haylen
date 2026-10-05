-- An error of code that runs every frame can resume: in development, saving the fix reloads the module and the app runs on from the error screen with its state, while an error of a step of the lifecycle restarts the app.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DevelopmentTest = require('categories.development.development-test')
local Test = require('harness.test')
local fault = require('categories.development.edit.fault')

local Resume = haylen.class('Resume', DevelopmentTest)

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

function Resume:draw(area)
    Test.caption(string.format('Frames updated %d', self.frames), area.width / 2, area.height / 2, {size = 40, color = Test.ink, anchor = {0.5, 0.5}})
    Test.caption(self.armed and 'Armed: the module raises until it is fixed.' or 'Not armed.', area.width / 2, area.height / 2 + 60, {size = 26, color = self.armed and Test.warm or Test.muted, anchor = {0.5, 0.5}})
end

return Resume
