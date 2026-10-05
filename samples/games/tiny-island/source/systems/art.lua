-- The art of the game: an atlas per character with one tagged animation per move, and the atlases of the props and the effects.
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

function art.atlas(path)
    return assets.load(path, 'atlas', art.options)
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
