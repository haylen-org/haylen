-- The textures and frame animations of the quest, cut from the sheets in content/images.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local art = {}

local function sheet(path, frameWidth, frameHeight, animations)
    local texture = assets.texture(path)
    local result = {texture = texture}
    for name, spec in pairs(animations) do
        result[name] = animation2d.fromGrid(texture, {frameWidth = frameWidth, frameHeight = frameHeight, cells = spec.frames, framesPerSecond = spec.fps, loop = spec.loop})
    end
    return result
end

function art.load()
    return {
        hero = sheet('images/hero.png', 24, 24, {
            idle = {frames = {1, 2}, fps = 2},
            walk = {frames = {3, 4, 5, 6}, fps = 8},
            attack = {frames = {7, 8, 9}, fps = 12, loop = 'once'},
            hurt = {frames = {10}},
        }),
        slime = sheet('images/slime.png', 18, 16, {move = {frames = {1, 2, 1, 3}, fps = 6}, hurt = {frames = {4}}}),
        mushroom = sheet('images/mushroom.png', 18, 16, {move = {frames = {2, 1, 3, 1}, fps = 6}, hurt = {frames = {4}}}),
        coin = sheet('images/coin.png', 10, 10, {spin = {fps = 10}}),
        decorations = sheet('images/decorations.png', 8, 8, {all = {}}),
        ground = assets.texture('images/ground.png'),
        grip = assets.texture('images/grip.png'),
        cloud = assets.texture('images/cloud.png'),
    }
end

return art
