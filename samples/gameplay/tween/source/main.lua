-- Haylen Tween: one scene per feature of haylen.tween, picked from a menu.
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {
    {name = 'back', type = 'button', bindings = {'key:escape', 'button:east', 'button:back'}},
    {name = 'replay', type = 'button', bindings = {'key:r', 'button:west'}},
}})

scene.push(require('scenes.menu')())
