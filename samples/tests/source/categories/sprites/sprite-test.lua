-- The base of the sprite tests: it preloads the images of the category in the `load` hook while the transition covers the screen, releases them when the test unloads, and reads the pointer on the stage.
local assets = require('haylen.assets')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')

local SpriteTest = haylen.class('SpriteTest', Test)

-- The art of the category is smooth, so every image filters linearly.
SpriteTest.textureOptions = {filter = 'linear'}

assets.defineGroups({groups = {sprites = {
    {path = 'sprites/images/', options = SpriteTest.textureOptions},
    {path = 'sprites/sheets/', options = SpriteTest.textureOptions},
    {path = 'sprites/atlases/gems.json', type = 'atlas', options = SpriteTest.textureOptions},
    {path = 'sprites/atlases/slime.json', type = 'atlas', options = SpriteTest.textureOptions},
}}})

function SpriteTest:load(context)
    context:preload('sprites'):await()
end

function SpriteTest:unload()
    assets.unloadGroup('sprites')
end

function SpriteTest.texture(path)
    return assets.texture('sprites/' .. path, SpriteTest.textureOptions)
end

function SpriteTest.atlas(path)
    return assets.load('sprites/' .. path, 'atlas', SpriteTest.textureOptions)
end

-- Returns the pointer in stage coordinates, whether it went down on the stage this frame and whether it is held there, from the first finger or the mouse, leaving out the interface.
function SpriteTest:pointer()
    local touch = input.touches()[1]
    local x, y
    if touch then
        x, y = self:toStage(touch.x, touch.y)
    else
        x, y = self:toStage(input.mousePosition())
    end
    local free = self.area ~= nil and self.area:contains({x, y}) and not ui.usingPointer()
    if touch then
        local held = free and touch.phase ~= 'ended' and touch.phase ~= 'cancelled'
        return x, y, held and touch.phase == 'began', held
    end
    return x, y, free and input.mousePressed('left'), free and input.mouseDown('left')
end

return SpriteTest
