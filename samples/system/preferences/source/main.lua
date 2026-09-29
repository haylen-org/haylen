-- Haylen Preferences: one scene per way of keeping the choices of the player between sessions, picked from a menu.
local localization = require('haylen.localization')
local scene = require('haylen.scene')

local settings = require('settings')

localization.loadFolder('locale')
localization.setFallback('en')
settings.start()

scene.push(require('scenes.menu')())
