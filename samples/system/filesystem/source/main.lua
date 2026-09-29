-- Haylen Filesystem: one scene per way of reading and writing files, picked from a menu.
local scene = require('haylen.scene')

scene.push(require('scenes.menu')())
