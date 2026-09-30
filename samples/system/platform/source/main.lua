-- Haylen Platform: one scene per part of the platform bridge and of what the window and the device report, picked from a menu. The native handlers of this app live in its local plugin, plugins/platform-sample.
local scene = require('haylen.scene')

scene.push(require('scenes.menu')())
