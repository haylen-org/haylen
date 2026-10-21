-- The base of the animation tests: it preloads the art of the category, the fighter, its training dummy, the puppet, the walker and the scenery they stand in, in the `load` hook while the transition covers the screen and releases it when the test unloads.
local assets = require('haylen.assets')
local haylen = require('haylen')

local Test = require('harness.test')

local AnimationTest = haylen.class('AnimationTest', Test)

AnimationTest.linear = {filter = 'linear'}
AnimationTest.mipmapped = {filter = 'linear', mipmaps = true}

assets.defineGroups({groups = {animation = {
    {path = 'animation/fighter.json', type = 'atlas', options = AnimationTest.linear},
    {path = 'animation/fighter.json', type = 'atlas', options = AnimationTest.mipmapped},
    {path = 'animation/dummy.png', options = AnimationTest.linear},
    {path = 'animation/puppet.skeleton', options = AnimationTest.linear},
    {path = 'sprites/sheets/walker.png', options = AnimationTest.linear},
    {path = 'camera/clouds.png', options = AnimationTest.linear},
    {path = 'camera/hills.png', options = AnimationTest.linear},
    {path = 'camera/ground.png', options = AnimationTest.linear},
    {path = 'tiled/tiles/terrain.png', options = AnimationTest.linear},
}}})

function AnimationTest:load(context)
    context:preload('animation'):await()
end

function AnimationTest:unload()
    assets.unloadGroup('animation')
end

function AnimationTest.atlas()
    return assets.load('animation/fighter.json', 'atlas', AnimationTest.linear)
end

return AnimationTest
