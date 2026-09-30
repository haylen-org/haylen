-- Haylen Plugins: the native-demo plugin of plugins/native-demo, with one scene per capability of native plugins, picked from a menu. The plugin answers in Swift on Apple platforms, Kotlin on Android, JavaScript on the web and C on the desktops.
local scene = require('haylen.scene')

-- Loading the Lua API of the plugin tells its native part that this app started, which hands it the error that stopped the app before a restart.
require('native-demo')

scene.push(require('scenes.menu')())
