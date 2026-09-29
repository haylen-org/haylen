-- Haylen Algorithms: one scene per algorithm of navigation2d, spatial2d, procedural2d, math and ai, picked from a menu.
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {
    -- The Back button of a gamepad goes back too, next to Escape and the east button that ui_cancel reads by default.
    {name = 'ui_cancel', type = 'button', bindings = {'key:escape', 'button:east', 'button:back'}},
    {name = 'reset', type = 'button', bindings = {'key:r', 'button:west'}},
    {name = 'previous', type = 'button', bindings = {'key:q', 'button:left_shoulder'}},
    {name = 'next', type = 'button', bindings = {'key:e', 'button:right_shoulder'}},
    {name = 'cursor', type = 'vector', bindings = {'stick:right'}},
    {name = 'press', type = 'button', bindings = {'axis:right_trigger+'}},
}})

scene.push(require('scenes.menu').new())
