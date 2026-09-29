-- Haylen Physics: one scene per feature of haylen.physics2d, picked from a menu.
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {
    -- The Back button of a gamepad goes back too, next to Escape and the east button that uiCancel reads by default.
    {name = 'uiCancel', type = 'button', bindings = {'key:escape', 'button:east', 'button:back'}},
    {name = 'reset', type = 'button', bindings = {'key:r', 'button:west'}},
    {name = 'previous', type = 'button', bindings = {'key:q', 'button:leftShoulder'}},
    {name = 'next', type = 'button', bindings = {'key:e', 'button:rightShoulder'}},
    {name = 'cursor', type = 'vector', bindings = {'stick:right'}},
    {name = 'press', type = 'button', bindings = {'axis:rightTrigger+'}},
    {name = 'move', type = 'vector', up = {'key:w', 'key:up'}, down = {'key:s', 'key:down'}, left = {'key:a', 'key:left'}, right = {'key:d', 'key:right'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'key:w', 'key:up', 'button:south', 'virtual:jump'}},
    {name = 'throttle', type = 'axis', positive = {'key:d', 'key:right', 'axis:rightTrigger+', 'axis:leftX+', 'virtual:gas'}, negative = {'key:a', 'key:left', 'axis:leftTrigger+', 'axis:leftX-', 'virtual:reverse'}},
}})

scene.push(require('scenes.menu').new())
