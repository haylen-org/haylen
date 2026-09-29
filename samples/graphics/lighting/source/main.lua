-- Haylen Lighting: a menu of 2D lighting tests, one scene per test.
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions('input/actions.json')

scene.push(require('scenes.menu')())
