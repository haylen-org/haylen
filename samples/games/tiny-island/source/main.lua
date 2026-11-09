-- Tiny Island: survive the nights on an island by keeping a campfire burning with the wood of its trees.
local assets = require('haylen.assets')
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local preferences = require('systems.preferences')

assets.defineGroups(assets.json('preload.json'))
input.loadActions(assets.json('input/actions.json'))
ui.setTheme(ui.loadTheme('ui/theme.json', 'dark'))
preferences.apply()

scene.push(require('scenes.boot').new())
