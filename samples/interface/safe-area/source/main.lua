-- Haylen Safe Area: a menu of tests of the safe area, the anchors of haylen.ui and the safe area simulation, one scene per test.
local input = require('haylen.input')
local scene = require('haylen.scene')

local sample = require('sample')

-- The Back button of a gamepad goes back too, next to Escape and the east button that ui_cancel reads by default.
input.defineAction({name = 'ui_cancel', type = 'button', bindings = {'key:escape', 'button:east', 'button:back'}})

sample.simulateOnDesktop()
scene.push(require('scenes.menu')())
