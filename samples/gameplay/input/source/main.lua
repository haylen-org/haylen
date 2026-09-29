-- Haylen Input: one scene per input feature, picked from a menu. The default action map loads first and the bindings the player remapped, kept in the preferences, replace it.
local input = require('haylen.input')
local preferences = require('haylen.preferences')
local scene = require('haylen.scene')

local controls = require('controls')

input.loadActions(controls.defaults())
preferences.apply()

scene.push(require('scenes.menu')())
