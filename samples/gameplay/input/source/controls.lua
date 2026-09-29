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
            up = {'key:w', 'key:up', 'button:dpad_up'},
            down = {'key:s', 'key:down', 'button:dpad_down'},
            left = {'key:a', 'key:left', 'button:dpad_left'},
            right = {'key:d', 'key:right', 'button:dpad_right'},
            bindings = {'stick:left', 'virtual_stick:move'},
        },
        {name = 'jump', type = 'button', bindings = {'key:space', 'mouse:left', 'button:south', 'virtual:jump'}},
        {name = 'dash', type = 'button', bindings = {'key:left_shift', 'mouse:right', 'button:west', 'virtual:dash'}},
        {name = 'throttle', type = 'axis', positive = {'key:e', 'axis:right_trigger+'}, negative = {'key:q', 'axis:left_trigger+'}},
    }}
end

return controls
