-- Haylen Physics: one scene per feature of haylen.physics2d, picked from a menu.
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {
    {name = 'back', type = 'button', bindings = {'key:escape', 'key:menu', 'button:east', 'button:back'}},
    {name = 'reset', type = 'button', bindings = {'key:r', 'button:west'}},
    {name = 'previous', type = 'button', bindings = {'key:q', 'button:left_shoulder'}},
    {name = 'next', type = 'button', bindings = {'key:e', 'button:right_shoulder'}},
    {name = 'cursor', type = 'vector', bindings = {'stick:right'}},
    {name = 'press', type = 'button', bindings = {'axis:right_trigger+'}},
    {name = 'move', type = 'vector', up = {'key:w', 'key:up'}, down = {'key:s', 'key:down'}, left = {'key:a', 'key:left'}, right = {'key:d', 'key:right'}, bindings = {'stick:left', 'virtual_stick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'key:w', 'key:up', 'button:south', 'virtual:jump'}},
    {name = 'throttle', type = 'axis', positive = {'key:d', 'key:right', 'axis:right_trigger+', 'axis:left_x+', 'virtual:gas'}, negative = {'key:a', 'key:left', 'axis:left_trigger+', 'axis:left_x-', 'virtual:reverse'}},
}})

scene.push(require('scenes.menu').new())
