-- The action map of the camera tests, which read the keyboard, gamepads and remotes while the play area has the focus.
return {actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left'}},
    {name = 'move2', type = 'vector', up = {'key:i'}, down = {'key:k'}, left = {'key:j'}, right = {'key:l'}, bindings = {'stick:right'}},
    {name = 'zoom', type = 'axis', positive = {'key:x', 'key:equal', 'key:keypadAdd', 'button:rightShoulder', 'axis:rightTrigger+'}, negative = {'key:z', 'key:minus', 'key:keypadSubtract', 'button:leftShoulder', 'axis:leftTrigger+'}},
    {name = 'rotate', type = 'axis', positive = {'key:e', 'axis:rightX+'}, negative = {'key:q', 'axis:rightX-'}},
    {name = 'point', type = 'button', bindings = {'mouse:left'}},
    {name = 'place', type = 'button', bindings = {'key:f', 'button:west'}},
}}
