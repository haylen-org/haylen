-- The way between the screens of the project: the menu of categories, the tests of a category and a test, with the last test opened kept in the preferences and the way back after an error.
local events = require('haylen.events')
local haylen = require('haylen')
local input = require('haylen.input')
local log = require('haylen.log')
local preferences = require('haylen.preferences')
local scene = require('haylen.scene')

local catalog = require('harness.catalog')

local navigation = {}

navigation.transition = {effect = 'fade', duration = 0.3, color = '#FF101418'}

-- The View button of gamepads and the Play/Pause button of TV remotes, which the remotes report as the pause key, move the focus between a test and its controls, next to Tab on keyboards.
navigation.focusAction = {name = 'uiFocus', type = 'button', bindings = {'button:back', 'key:pause'}}

-- Gives the app the action map of the project: the actions a test loads, or none, plus the action that moves the focus between a test and its controls.
function navigation.useActions(document)
    if document then
        input.loadActions(document)
    else
        input.clearActions()
    end
    input.defineAction(navigation.focusAction)
end

function navigation.lastTest()
    return catalog.byCode[preferences.get('tests.last', '')]
end

function navigation.openCategory(category)
    if not scene.transitioning() then
        scene.push(require('harness.category')(category), navigation.transition)
    end
end

-- Opens a test, or the page that says why it cannot run here, and remembers it for the next launch.
function navigation.openTest(test)
    if scene.transitioning() then
        return
    end
    preferences.set('tests.last', test.code)
    preferences.save()
    local reason = catalog.unsupported(test)
    if reason then
        log.info(string.format('[%s] Unsupported on "%s": %s', test.code, haylen.platform, reason))
        scene.push(require('harness.unsupported')(test, reason), navigation.transition)
        return
    end
    log.info(string.format('[%s] Opened "%s".', test.code, test.title))
    scene.push(require(test.module)(test), navigation.transition)
end

function navigation.back()
    if not scene.transitioning() then
        scene.pop(navigation.transition)
    end
end

-- Opens the menu with the test list of the category of `test` on top, focused on the test, the way the project comes back from the error screen.
function navigation.reopen(test)
    scene.clear()
    navigation.useActions(nil)
    scene.push(require('harness.menu')(test and test.category))
    if test then
        scene.push(require('harness.category')(test.category, test))
    end
end

-- The error screen of the engine offers to go back, which returns to the test list of the test that failed instead of restarting the project.
function navigation.install()
    haylen.setRecoverable(true)
    events.on('appError', function(error)
        local test = navigation.current
        log.error(string.format('[%s] Error: %s', test and test.code or 'Menu', error.message))
    end)
    events.on('appRecovered', function()
        local test = navigation.current
        navigation.current = nil
        navigation.reopen(test)
    end)
end

return navigation
