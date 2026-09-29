-- Haylen Events: one scene per feature of haylen.signal, haylen.events and the lifecycle of the engine, picked from a menu.
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {
    -- The Back button of a gamepad goes back too, next to Escape and the east button that ui_cancel reads by default.
    {name = 'ui_cancel', type = 'button', bindings = {'key:escape', 'button:east', 'button:back'}},
    {name = 'pause', type = 'button', bindings = {'key:p', 'button:start'}},
}})

scene.push(require('scenes.menu')())
