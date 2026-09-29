-- Haylen Nine-Patch: one scene per feature of nine-slice frames, picked from a menu.
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {
    -- The Back button of a gamepad goes back too, next to Escape and the east button that ui_cancel reads by default.
    {name = 'ui_cancel', type = 'button', bindings = {'key:escape', 'button:east', 'button:back'}},
    {name = 'resize', type = 'vector', up = {'key:w'}, down = {'key:s'}, left = {'key:a'}, right = {'key:d'}, bindings = {'stick:right'}},
}})

scene.push(require('scenes.menu')())
