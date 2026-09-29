-- The action map of the sample. Gameplay tests read these actions, so every key, mouse button, gamepad control and touch control drives them without extra code.
local controls = {}

-- The actions and binding lists the remapping test lets the player change, with one keyboard or mouse binding and one gamepad binding each.
controls.remappable = {
    {action = 'move', list = 'up', title = 'Move up'},
    {action = 'move', list = 'down', title = 'Move down'},
    {action = 'move', list = 'left', title = 'Move left'},
    {action = 'move', list = 'right', title = 'Move right'},
    {action = 'jump', list = 'bindings', title = 'Jump'},
    {action = 'dash', list = 'bindings', title = 'Dash'},
}

-- Returns a new copy of the default action map, since input.loadActions keeps no reference to it.
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

return controls
