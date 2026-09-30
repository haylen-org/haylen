-- Haylen Native: the test library of the engine called from Lua on every platform, one scene per part of `haylen.native` and of the bridge, each with a list of checks that pass or fail. The library comes from `native/`, the platform handlers live in the local plugin `plugins/native-sample`, and the local plugin `plugins/native-test` stands in for the library in the browser.
local scene = require('haylen.scene')

scene.push(require('scenes.menu')())
