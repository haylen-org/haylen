-- Haylen UI: a menu of haylen.ui tests, one scene per group of components.
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

-- The Back button of a gamepad goes back too, next to Escape and the east button that uiCancel reads by default.
input.defineAction({name = 'uiCancel', type = 'button', bindings = {'key:escape', 'button:east', 'button:back'}})

-- The textured theme registers its fonts at start, so the rich text test can name them before the theme is shown.
ui.loadTheme('themes/parchment.json', 'light')

scene.push(require('scenes.menu')())
