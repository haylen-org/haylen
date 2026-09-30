-- Plays the game sounds by name, picking a random variant when a sound has several recordings.
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local sound = {}

local effects = {
    click = {bus = 'ui', files = {'ui/click.ogg'}},
    confirm = {bus = 'ui', files = {'ui/confirm.ogg'}},
    back = {bus = 'ui', files = {'ui/back.ogg'}},
    error = {bus = 'ui', files = {'ui/error.ogg'}},
    chop = {bus = 'sfx', files = {'effects/chop.ogg', 'effects/wood_hit_1.ogg', 'effects/wood_hit_2.ogg', 'effects/wood_hit_3.ogg'}},
    treeFall = {bus = 'sfx', files = {'effects/tree_fall.ogg'}},
    pickup = {bus = 'sfx', files = {'effects/wood_pickup.ogg'}, volume = 0.7},
    feed = {bus = 'sfx', files = {'effects/fire_feed.ogg'}},
    explosion = {bus = 'sfx', files = {'effects/explosion.ogg'}},
    swing = {bus = 'sfx', files = {'effects/swing_1.wav', 'effects/swing_2.wav', 'effects/swing_3.wav'}, volume = 0.6},
    arrow = {bus = 'sfx', files = {'effects/arrow.wav'}, volume = 0.6},
    hit = {bus = 'sfx', files = {'effects/hit_1.ogg', 'effects/hit_2.ogg', 'effects/hit_3.ogg'}},
    guard = {bus = 'sfx', files = {'effects/guard.ogg'}},
    playerHurt = {bus = 'sfx', files = {'effects/player_hurt.ogg'}},
    enemyHurt = {bus = 'sfx', files = {'effects/enemy_hurt.ogg'}, volume = 0.6},
    enemyDie = {bus = 'sfx', files = {'effects/enemy_die.ogg'}},
    heal = {bus = 'sfx', files = {'effects/heal.wav'}},
    footstep = {bus = 'sfx', files = {'effects/footstep_1.ogg', 'effects/footstep_2.ogg', 'effects/footstep_3.ogg', 'effects/footstep_4.ogg'}, volume = 0.35},
    dawn = {bus = 'ui', files = {'jingles/dawn.ogg'}},
    dusk = {bus = 'ui', files = {'jingles/dusk.ogg'}},
}

local music = {menu = 'music/menu.mp3', day = 'music/day.mp3', gameOver = 'music/game_over.mp3', night = 'ambient/night.ogg'}

local function load(file)
    return assets.load('audio/' .. file)
end

-- Plays an effect, positioned in the world when `x` and `y` are given so the listener hears where it happened.
function sound.play(name, x, y)
    local effect = effects[name]
    local file = effect.files[math.random(#effect.files)]
    local options = {bus = effect.bus, volume = effect.volume or 1, pitchVariation = 0.08}
    if x then
        options.x = x
        options.y = y
    end
    return audio.play(load(file), options)
end

function sound.loop(file, volume)
    return audio.play(load(file), {bus = 'ambience', volume = volume, loop = true, fadeIn = 1})
end

-- Crossfades to a music track, which streams from its encoded file instead of being decoded up front.
function sound.music(name, fade)
    local track = assets.load('audio/' .. music[name], 'sound', {stream = true})
    audio.playMusic(track, {fade = fade or 1.5, loop = name ~= 'gameOver'})
end

function sound.stop(voice)
    audio.stop(voice, 0.5)
end

return sound
