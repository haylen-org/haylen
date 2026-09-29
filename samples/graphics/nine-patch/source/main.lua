-- Haylen Nine-Patch: one scene per feature of nine-slice frames, picked from a menu.
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {
    {name = 'back', type = 'button', bindings = {'key:escape', 'button:east', 'button:back'}},
    {name = 'resize', type = 'vector', up = {'key:w'}, down = {'key:s'}, left = {'key:a'}, right = {'key:d'}, bindings = {'stick:right'}},
}})

scene.push(require('scenes.menu')())
