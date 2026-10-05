-- The action map of the input tests. The tests that read actions load it, with the bindings the remapping test stored in the preferences in place of the defaults, so every key, mouse button, gamepad control and touch control drives them without extra code.
local input = require('haylen.input')
local preferences = require('haylen.preferences')

local controls = {}

controls.preference = 'tests.input.actions'

-- The actions and binding lists the remapping test lets the player change, with one keyboard or mouse binding and one gamepad binding each.
controls.remappable = {
    {action = 'move', list = 'up', title = 'Move up'},
    {action = 'move', list = 'down', title = 'Move down'},
    {action = 'move', list = 'left', title = 'Move left'},
    {action = 'move', list = 'right', title = 'Move right'},
    {action = 'jump', list = 'bindings', title = 'Jump'},
    {action = 'dash', list = 'bindings', title = 'Dash'},
}

-- Returns a new copy of the default action map, since `input.loadActions` keeps no reference to it.
function controls.defaults()
    return {actions = {
        {
            name = 'move',
            type = 'vector',
            up = {'key:w', 'key:up', 'button:dpadUp'},
            down = {'key:s', 'key:down', 'button:dpadDown'},
            left = {'key:a', 'key:left', 'button:dpadLeft'},
            right = {'key:d', 'key:right', 'button:dpadRight'},
            bindings = {'stick:left', 'virtualStick:move'},
        },
        {name = 'jump', type = 'button', bindings = {'key:space', 'mouse:left', 'button:south', 'virtual:jump'}},
        {name = 'dash', type = 'button', bindings = {'key:leftShift', 'mouse:right', 'button:west', 'virtual:dash'}},
        {name = 'throttle', type = 'axis', positive = {'key:e', 'axis:rightTrigger+'}, negative = {'key:q', 'axis:leftTrigger+'}},
    }}
end

-- Returns the action map the tests load: the one the remapping test stored, or the defaults.
function controls.actions()
    return preferences.get(controls.preference) or controls.defaults()
end

-- Stores the actions of the current map that the defaults define, leaving out the action the project adds to move the focus.
function controls.save()
    local document = {actions = {}}
    for _, action in ipairs(controls.defaults().actions) do
        document.actions[#document.actions + 1] = input.actionDefinition(action.name)
    end
    preferences.set(controls.preference, document)
    preferences.save()
end

-- Forgets the stored bindings, so the tests load the defaults again.
function controls.forget()
    preferences.remove(controls.preference)
    preferences.save()
end

function controls.saved()
    return preferences.has(controls.preference)
end

return controls
