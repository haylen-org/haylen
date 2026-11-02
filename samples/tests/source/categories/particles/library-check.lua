-- Library check: loads every effect file of `content/particles/library` in the background, creates its emitters and runs them for a second, and lists the code of this test with the file and the error of every effect that fails, so the library never holds a broken file. The check raises an error at the end when any file failed, which marks the test as failed in an automatic run.
local assets = require('haylen.assets')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local LibraryCheck = haylen.class('LibraryCheck', ParticleTest)

LibraryCheck.code = [[
local load = assets.loadAsync('particles/library/fire/torch-flame.particles')  -- Decodes on a worker thread.
local ok, problem = pcall(function()
    local system = particles2d.newSystem(load:await(), {seed = 1})  -- Validates every value of the effect.
    for _ = 1, 30 do system:update(1 / 30) end
end)]]

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
    self.emitterCount = 0
    self.failures = {}
    self:groupByCategory()
end

-- Sorts the files into the folders of their categories, which the grid shows with how many of their files ran.
function LibraryCheck:groupByCategory()
    self.categories, self.categoryOf = {}, {}
    local byId = {}
    for _, path in ipairs(self.paths) do
        local id = path:match('^' .. LibraryCheck.folder .. '/([^/]+)/')
        local category = byId[id]
        if not category then
            category = {id = id, title = id:sub(1, 1):upper() .. id:sub(2):gsub('%-', ' '), total = 0, checked = 0, failed = 0}
            byId[id] = category
            self.categories[#self.categories + 1] = category
        end
        category.total = category.total + 1
        self.categoryOf[path] = category
    end
    table.sort(self.categories, function(a, b) return a.title < b.title end)
end

function LibraryCheck:categoryCells()
    local cells = {}
    for index, category in ipairs(self.categories) do
        cells[index] = ui.card{padding = {10, 14}, ui.row{gap = 8,
            ui.label{text = category.title, grow = 1},
            ui.badge{id = 'category-' .. category.id, text = string.format('0 of %d', category.total)},
        }}
    end
    return cells
end

function LibraryCheck:showCategory(category)
    local tone = category.failed > 0 and 'danger' or category.checked == category.total and 'success' or 'neutral'
    self:set('category-' .. category.id, {text = string.format('%d of %d', category.checked, category.total), tone = tone})
end

-- Creates the emitters of an effect and runs them, which validates every value the effect sets, and returns how many emitters it has.
function LibraryCheck.exercise(effect)
    local system = particles2d.newSystem(effect, {seed = 1})
    for _ = 1, LibraryCheck.steps do
        system:update(1 / LibraryCheck.steps)
    end
    return #system:emitters()
end

function LibraryCheck:enter()
    self:frame{
        stage = false,
        code = LibraryCheck.code,
        content = {
            ui.label{id = 'summary', text = string.format('Checking %d effect files.', #self.paths), font = 'heading'},
            ui.progress{id = 'progress', value = 0},
            ui.column{id = 'failures', gap = 8},
            ui.grid{minColumnWidth = 230, columns = 6, gap = 10, children = self:categoryCells()},
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
                self.emitterCount = self.emitterCount + LibraryCheck.exercise(loads[index]:await())
            end)
            local category = self.categoryOf[path]
            if not ok then
                self.failures[#self.failures + 1] = {path = path, message = tostring(problem)}
                category.failed = category.failed + 1
                self:log('Failed: File "%s": %s', path, tostring(problem))
            end
            self.checked = self.checked + 1
            category.checked = category.checked + 1
            self:showCategory(category)
            self:set('progress', {value = self.checked / #self.paths})
        end
        self:finish()
    end)
end

-- Groups the failures by their message, so one cause that breaks many files reads as one line with the files it broke.
function LibraryCheck.group(failures)
    local groups, byMessage = {}, {}
    for _, failure in ipairs(failures) do
        local group = byMessage[failure.message]
        if not group then
            group = {message = failure.message, paths = {}}
            byMessage[failure.message] = group
            groups[#groups + 1] = group
        end
        group.paths[#group.paths + 1] = failure.path
    end
    return groups
end

function LibraryCheck:finish()
    local rows = {}
    for index, group in ipairs(LibraryCheck.group(self.failures)) do
        local files = #group.paths == 1 and string.format('The file "%s"', group.paths[1]) or string.format('%d files, the first one "%s",', #group.paths, group.paths[1])
        rows[index] = ui.label{text = string.format('%s failed: %s', files, group.message), color = 'dangerText'}
    end
    self.gui:replaceChildren('failures', rows)
    local summary = string.format('Checked %d effect files, and %d failed.', self.checked, #self.failures)
    self:set('summary', {text = summary})
    self:log(summary)
    if #self.failures > 0 then
        error(string.format('The effect library has %d broken files, the first one "%s".', #self.failures, self.failures[1].path))
    end
end

-- The status tells what the check does while it runs, and what the files hold once it ends, so it never repeats the summary.
function LibraryCheck:update(dt)
    LibraryCheck.super.update(self, dt)
    if self.checked < #self.paths then
        self:status(string.format('Loading, building and running the file "%s".', self.paths[self.checked + 1]))
    elseif #self.failures == 0 then
        self:status(string.format('The %d files built %d emitters, and every one ran for a second without an error.', #self.paths, self.emitterCount))
    else
        self:status(string.format('The files that ran built %d emitters, and the ones listed above failed.', self.emitterCount))
    end
end

return LibraryCheck
