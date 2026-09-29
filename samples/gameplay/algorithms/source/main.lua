-- Haylen Algorithms: one scene per algorithm of navigation2d, spatial2d, procedural2d, math and ai, picked from a menu.
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {
    {name = 'back', type = 'button', bindings = {'key:escape', 'key:menu', 'button:east', 'button:back'}},
    {name = 'reset', type = 'button', bindings = {'key:r', 'button:west'}},
    {name = 'previous', type = 'button', bindings = {'key:q', 'button:left_shoulder'}},
    {name = 'next', type = 'button', bindings = {'key:e', 'button:right_shoulder'}},
    {name = 'cursor', type = 'vector', bindings = {'stick:right'}},
    {name = 'press', type = 'button', bindings = {'axis:right_trigger+'}},
}})

scene.push(require('scenes.menu').new())
