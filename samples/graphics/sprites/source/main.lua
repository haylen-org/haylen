-- Haylen Sprites: one scene per feature of 2D drawing, picked from a menu. Every test preloads the images of the sample while its transition covers the screen.
local assets = require('haylen.assets')
local input = require('haylen.input')
local scene = require('haylen.scene')

local sample = require('sample')

assets.defineGroups({groups = {sprites = {
    {path = 'images/', options = sample.textureOptions},
    {path = 'sheets/', options = sample.textureOptions},
    {path = 'atlases/gems.json', type = 'atlas', options = sample.textureOptions},
    {path = 'atlases/slime.json', type = 'atlas', options = sample.textureOptions},
}}})

input.loadActions({actions = {
    -- The Back button of a gamepad goes back too, next to Escape and the east button that uiCancel reads by default.
    {name = 'uiCancel', type = 'button', bindings = {'key:escape', 'button:east', 'button:back'}},
    {name = 'add', type = 'button', bindings = {'key:equal', 'key:keypadAdd', 'button:rightShoulder'}},
    {name = 'remove', type = 'button', bindings = {'key:minus', 'key:keypadSubtract', 'button:leftShoulder'}},
}})

scene.push(require('scenes.menu')())
