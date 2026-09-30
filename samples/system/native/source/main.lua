-- Haylen Native: the test library of the engine called from Lua on every platform, one scene per part of `haylen.native` and of the bridge, each with a list of checks that pass or fail. The library comes from `native/`, and the platform handlers live in `platform/android`, `platform/apple` and `platform/web`.
local scene = require('haylen.scene')

scene.push(require('scenes.menu')())
