-- Haylen Orientation: a menu of tests of the screen orientation, its lock and the design resolution, one scene per test.
local input = require('haylen.input')
local scene = require('haylen.scene')

-- The Back button of a gamepad goes back too, next to Escape and the east button that `uiCancel` reads by default.
input.defineAction({name = 'uiCancel', type = 'button', bindings = {'key:escape', 'button:east', 'button:back'}})

scene.push(require('scenes.menu')())
