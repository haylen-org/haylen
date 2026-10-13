-- The base of the animation tests: it preloads the art of the category in the `load` hook while the transition covers the screen and releases it when the test unloads.
local assets = require('haylen.assets')
local haylen = require('haylen')

local Test = require('harness.test')

local AnimationTest = haylen.class('AnimationTest', Test)

assets.defineGroups({groups = {animation = {{path = 'animation/fighter.json', type = 'atlas', options = {filter = 'linear'}}}}})

function AnimationTest:load(context)
    context:preload('animation'):await()
end

function AnimationTest:unload()
    assets.unloadGroup('animation')
end

function AnimationTest.atlas()
    return assets.load('animation/fighter.json', 'atlas', {filter = 'linear'})
end

return AnimationTest
