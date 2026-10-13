-- The art of the game: an atlas per character with one tagged animation per move, and the atlases of the props and the effects, all packed into one texture when the menu loads, so the sprites of the island draw from one texture and batch together.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local art = {}

-- The art is painted in high definition, so every image is smoothed when the screen scales it.
art.options = {filter = 'linear'}

-- How large each character is drawn and where the ground is in its frames, as a fraction of the frame height.
local characters = {
    warrior = {scale = 0.82, pivotY = 178 / 192},
    archer = {scale = 0.82, pivotY = 178 / 192},
    lancer = {scale = 0.82, pivotY = 178 / 192},
    mage = {scale = 0.82, pivotY = 178 / 192},
    brute = {scale = 0.82, pivotY = 272 / 288},
    thrower = {scale = 0.82, pivotY = 178 / 192},
    bomber = {scale = 0.82, pivotY = 178 / 192},
    imp = {scale = 0.82, pivotY = 146 / 160},
    sheep = {scale = 0.8, pivotY = 146 / 160},
}

-- The atlases the packed texture holds, each under its path as a prefix.
local packedAtlases = {'world/props', 'effects/effects', 'characters/warrior', 'characters/archer', 'characters/lancer', 'characters/mage', 'characters/sheep', 'characters/imp', 'characters/brute', 'characters/thrower', 'characters/bomber'}
local packed
local subsets = {}

-- Returns what a promise gives, or raises its failure.
local function awaited(promise)
    local value, failure = promise:await()
    if value == nil then
        error(failure, 0)
    end
    return value
end

-- Packs every atlas into one texture on the task pool the first time a coroutine calls it, reading the atlases and their images without blocking, and lets their own textures go once they are packed.
function art.pack()
    if packed then
        return
    end
    local builder = animation2d.newAtlasBuilder()
    for _, path in ipairs(packedAtlases) do
        local atlas = awaited(assets.loadAsync(path .. '.json', 'atlas', art.options))
        builder:addAtlas(path .. '/', atlas, awaited(assets.loadAsync(path .. '.png', 'image')))
    end
    packed = awaited(builder:buildAsync({padding = 4, extrude = 2, filter = 'linear'}))
    assets.releaseUnused()
end

-- Returns one of the packed atlases, named as it was in its own file.
function art.atlas(path)
    local name = path:gsub('%.json$', '')
    subsets[name] = subsets[name] or packed:subset(name .. '/')
    return subsets[name]
end

function art.texture(path)
    return assets.texture(path, art.options)
end

function art.props()
    return art.atlas('world/props.json')
end

function art.effects()
    return art.atlas('effects/effects.json')
end

-- Returns a character with its atlas, the scale it is drawn at and its ground line.
function art.character(name)
    local spec = characters[name]
    return {atlas = art.atlas('characters/' .. name .. '.json'), scale = spec.scale, pivotY = spec.pivotY}
end

-- The animator puts the feet of the character on the sprite position whenever it applies a frame. Every atlas tag is an animation, so gameplay code plays `run` or `attack` without knowing the character.
function art.newAnimator(name)
    local character = art.character(name)
    local animator = animation2d.newAnimator()
    animator.pivotY = character.pivotY
    for _, tag in ipairs(character.atlas:animationNames()) do
        animator:add(tag, character.atlas:animation(tag))
    end
    animator:play('idle')
    return animator, character
end

-- Builds an animation from the frames `<name>_1`, `<name>_2` and on of the effects atlas.
function art.effect(name, framesPerSecond, loop)
    local atlas = art.effects()
    local frames = {}
    while atlas:hasFrame(name .. '_' .. (#frames + 1)) do
        frames[#frames + 1] = atlas:source(name .. '_' .. (#frames + 1))
    end
    return animation2d.fromFrames(atlas.texture, frames, {framesPerSecond = framesPerSecond, loop = loop or 'once'})
end

-- Returns the path of the portrait of a class, for the UI.
function art.portrait(class)
    return 'ui/portraits/' .. class.id .. '.png'
end

function art.icon(name)
    return 'ui/icons/' .. name .. '.png'
end

return art
