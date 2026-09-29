-- Haylen Localization: one scene per feature of translated text, picked from a menu.
local input = require('haylen.input')
local scene = require('haylen.scene')

local language = require('language')

input.loadActions({actions = {
    {name = 'nextLanguage', type = 'button', bindings = {'key:l', 'button:right_shoulder'}},
    {name = 'previousLanguage', type = 'button', bindings = {'key:k', 'button:left_shoulder'}},
}})
language.setup()

scene.push(require('scenes.menu')())
