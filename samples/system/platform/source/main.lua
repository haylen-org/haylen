-- Haylen Platform: one scene per part of the platform bridge and of what the window and the device report, picked from a menu. The native handlers of this app live in platform/android, platform/apple and platform/web.
local scene = require('haylen.scene')

scene.push(require('scenes.menu')())
