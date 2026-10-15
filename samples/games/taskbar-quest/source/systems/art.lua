-- The textures and frame animations of the quest, all cut from the atlas `content/images/quest.json`, which loads with the linear filter because the art is painted in high definition.
local assets = require('haylen.assets')

local art = {}

function art.load()
    local atlas = assets.load('images/quest.json', 'atlas', {filter = 'linear'})
    local texture = atlas.texture
    return {
        texture = texture,
        hero = {texture = texture, idle = atlas:animation('hero idle'), walk = atlas:animation('hero walk'), attack = atlas:animation('hero attack'), hurt = atlas:animation('hero hurt')},
        slime = {texture = texture, move = atlas:animation('slime move'), hurt = atlas:animation('slime hurt')},
        mushroom = {texture = texture, move = atlas:animation('mushroom move'), hurt = atlas:animation('mushroom hurt')},
        coin = {texture = texture, spin = atlas:animation('coin spin')},
        decorations = {texture = texture, all = atlas:animation('decorations')},
        ground = atlas:source('ground'),
        grip = atlas:source('grip'),
        cloud = atlas:source('cloud'),
        dot = atlas:source('dot'),
    }
end

return art
