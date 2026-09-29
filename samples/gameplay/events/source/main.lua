-- Haylen Events: one scene per feature of haylen.signal, haylen.events and the lifecycle of the engine, picked from a menu.
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {
    {name = 'back', type = 'button', bindings = {'key:escape', 'button:east', 'button:back'}},
    {name = 'pause', type = 'button', bindings = {'key:p', 'button:start'}},
}})

scene.push(require('scenes.menu')())
