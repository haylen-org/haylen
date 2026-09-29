-- Haylen Audio: one scene per audio feature, picked from a menu. Every test preloads the sounds of the sample while its transition covers the screen.
local input = require('haylen.input')
local scene = require('haylen.scene')

local sounds = require('sounds')

sounds.defineGroup()

-- The listener of the positional test walks with WASD, the right stick or its touch stick, which leaves the arrows, the d-pad and the left stick to the focus of the panel.
input.loadActions({actions = {
    {name = 'walk', type = 'vector', up = {'key:w'}, down = {'key:s'}, left = {'key:a'}, right = {'key:d'}, bindings = {'stick:right', 'virtual_stick:walk'}},
}})

scene.push(require('scenes.menu')())
