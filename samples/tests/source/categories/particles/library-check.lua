-- Library check: loads every effect file of `content/particles/library` in the background, creates its emitters and runs them for a second, and lists the code of this test with the file and the error of every effect that fails, so the library never holds a broken file. The check raises an error at the end when any file failed, which marks the test as failed in an automatic run.
local assets = require('haylen.assets')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local LibraryCheck = haylen.class('LibraryCheck', ParticleTest)

LibraryCheck.folder = 'particles/library'
LibraryCheck.steps = 30

function LibraryCheck:init(entry)
    LibraryCheck.super.init(self, entry)
    self.paths = {}
    for _, path in ipairs(assets.list(LibraryCheck.folder)) do
        if assets.typeForPath(path) == 'particles' then
            self.paths[#self.paths + 1] = path
        end
    end
    self.checked = 0
    self.failures = {}
end

-- Creates the emitters of an effect and runs them, which validates every value the effect sets.
function LibraryCheck.exercise(effect)
    local system = particles2d.newSystem(effect, {seed = 1})
    for _ = 1, LibraryCheck.steps do
        system:update(1 / LibraryCheck.steps)
    end
    return system.count
end

function LibraryCheck:enter()
    self:frame{
        stage = false,
        content = {
            ui.label{id = 'summary', text = string.format('Checking %d effect files.', #self.paths), font = 'heading'},
            ui.progress{id = 'progress', value = 0},
            ui.scroll{grow = 1, ui.column{id = 'failures', gap = 8}},
        },
    }
    self:spawn(function()
        -- Every file loads on the worker threads at once, and the check takes the results in order.
        local loads = {}
        for index, path in ipairs(self.paths) do
            loads[index] = assets.loadAsync(path)
        end
        for index, path in ipairs(self.paths) do
            local ok, problem = pcall(function()
                LibraryCheck.exercise(loads[index]:await())
            end)
            if not ok then
                self.failures[#self.failures + 1] = {path = path, message = tostring(problem)}
                self:log('Failed: File "%s": %s', path, tostring(problem))
            end
            self.checked = self.checked + 1
            self:set('progress', {value = self.checked / #self.paths})
        end
        self:finish()
    end)
end

function LibraryCheck:finish()
    local rows = {}
    for index, failure in ipairs(self.failures) do
        rows[index] = ui.label{text = string.format('File "%s": %s', failure.path, failure.message), color = 'dangerText'}
    end
    self.gui:replaceChildren('failures', rows)
    local summary = string.format('Checked %d effect files, and %d failed.', self.checked, #self.failures)
    self:set('summary', {text = summary})
    self:log(summary)
    if #self.failures > 0 then
        error(string.format('The effect library has %d broken files, the first one "%s".', #self.failures, self.failures[1].path))
    end
end

function LibraryCheck:update(dt)
    LibraryCheck.super.update(self, dt)
    self:status(string.format('Checked %d of %d   Failed %d', self.checked, #self.paths, #self.failures))
end

return LibraryCheck
