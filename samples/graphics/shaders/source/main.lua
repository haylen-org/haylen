-- Haylen Shaders: a menu of custom shader tests, one scene per test.
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions('input/actions.json')

scene.push(require('scenes.menu')())
