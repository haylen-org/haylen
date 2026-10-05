-- Runs every test that this platform supports, one after the other for a few seconds each, and lists the tests that raised an error or whose scene failed to load with their codes. The menu starts it with "Run all", and the headless platform starts it by itself and quits once every test ran.
local events = require('haylen.events')
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local ui = require('haylen.ui')
local window = require('haylen.window')

local catalog = require('harness.catalog')
local navigation = require('harness.navigation')

local runner = {}

runner.seconds = 2

-- The screen under the tests while they run, with the progress, and the failures once every test ran.
local Progress = haylen.class('Progress', scene.Scene)

function Progress:enter()
    window.setBackLeavesApp(false)
    self.gui = ui.mount(ui.column{
        padding = {32, 48},
        gap = 24,
        onCancel = function() runner.stop() end,
        ui.row{gap = 24, align = 'start',
            ui.button{id = 'back', text = 'Back', onClick = function() runner.stop() end},
            ui.pageHeader{title = 'Run all', caption = string.format('Every test that runs on "%s", for %d seconds each.', haylen.platform, runner.seconds), grow = 1},
        },
        ui.label{id = 'progress', text = '', font = 'heading'},
        ui.scroll{grow = 1, ui.column{id = 'failures', gap = 8, padding = {0, 24, 0, 0}}},
    }, {owner = self})
    self.gui:command('back', 'focus')
    runner.show()
end

-- Shows how far the run is, and the failures so far.
function runner.show()
    local gui = runner.screen and runner.screen.gui
    if not gui then
        return
    end
    local total = #runner.queue
    local text = runner.index > total and string.format('Done: %d tests ran, %d failed.', total, #runner.failures) or string.format('Running %d of %d, %d failed so far.', runner.index, total, #runner.failures)
    gui:set('progress', {text = text})
    local rows = {}
    for index, failure in ipairs(runner.failures) do
        rows[index] = ui.label{text = string.format('%s  %s: %s', failure.test.code, failure.test.title, failure.message), color = 'dangerText'}
    end
    gui:replaceChildren('failures', rows)
end

-- Puts the progress screen alone on the stack, dropping whatever the last test left there.
function runner.reset()
    scene.clear()
    navigation.useActions(nil)
    runner.screen = Progress()
    scene.push(runner.screen)
end

function runner.start()
    runner.queue = {}
    for _, test in ipairs(catalog.tests) do
        if not catalog.unsupported(test) then
            runner.queue[#runner.queue + 1] = test
        end
    end
    runner.index = 0
    runner.failures = {}

    -- An error of a test is written down with its code, and the run goes on from the progress screen with the next test.
    runner.connections = {}
    runner.connections[1] = events.on('appError', function(error)
        local test = runner.queue[runner.index]
        runner.failures[#runner.failures + 1] = {test = test, message = error.message}
        log.error(string.format('[%s] Failed: %s', test.code, error.message))
        haylen.recover()
        return true
    end, {priority = 10})
    runner.connections[2] = events.on('appRecovered', function()
        runner.reset()
        runner.next()
        return true
    end, {priority = 10})
    -- A test whose scene fails to load fails, and the progress screen stays until the test's time ends.
    runner.connections[3] = events.on('sceneLoadFailed', function(event)
        if event.scene ~= runner.current then
            return
        end
        local test = runner.queue[runner.index]
        runner.failures[#runner.failures + 1] = {test = test, message = event.error}
        log.error(string.format('[%s] Failed to load: %s', test.code, event.error))
        runner.show()
    end, {priority = 10})

    log.info(string.format('Running %d tests on "%s".', #runner.queue, haylen.platform))
    runner.reset()
    runner.next()
end

function runner.next()
    if runner.timer then
        timer.cancel(runner.timer)
    end
    runner.index = runner.index + 1
    runner.show()
    local test = runner.queue[runner.index]
    if not test then
        runner.finish()
        return
    end

    log.info(string.format('[%s] Running "%s".', test.code, test.title))
    runner.current = require(test.module)(test)
    scene.push(runner.current)
    -- The run counts real frames whatever a test does to the time scale or the game pause.
    runner.timer = timer.after(runner.seconds, function()
        runner.timer = nil
        local failure = runner.failures[#runner.failures]
        if not failure or failure.test ~= test then
            log.info(string.format('[%s] Passed.', test.code))
        end
        scene.popTo(1)
        runner.next()
    end, {processMode = 'always', unscaled = true})
end

-- The headless platform quits once every test ran, with the status 1 when any test failed, which the headless player exits with.
function runner.finish()
    local codes = {}
    for index, failure in ipairs(runner.failures) do
        codes[index] = failure.test.code
    end
    runner.disconnect()
    if #codes == 0 then
        log.info(string.format('Ran %d tests, and none failed.', #runner.queue))
    else
        log.error(string.format('Ran %d tests, and %d failed: %s.', #runner.queue, #codes, table.concat(codes, ', ')))
    end
    if haylen.platform == 'headless' then
        haylen.quit({status = #codes == 0 and 0 or 1})
    end
end

function runner.disconnect()
    for _, connection in ipairs(runner.connections) do
        connection:disconnect()
    end
    runner.connections = {}
end

-- Leaves the run for the menu.
function runner.stop()
    if runner.timer then
        timer.cancel(runner.timer)
        runner.timer = nil
    end
    runner.disconnect()
    navigation.reopen(nil)
end

return runner
