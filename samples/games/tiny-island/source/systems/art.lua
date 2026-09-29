-- Builds animations from the Tiny Swords strips, where the frames of a strip share one size and sit left to right.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local art = {}

local root = 'tiny_swords/'

-- Clip names are shared by every unit, so gameplay code plays 'run' or 'attack' without knowing the unit.
local units = {
    warrior = {frame = 192, pivotY = 0.71, clips = {
        idle = {file = 'warrior/warrior_idle', fps = 10},
        run = {file = 'warrior/warrior_run', fps = 12},
        attack = {file = 'warrior/warrior_attack1', fps = 14, loop = false},
        attack2 = {file = 'warrior/warrior_attack2', fps = 14, loop = false},
        guard = {file = 'warrior/warrior_guard', fps = 10},
    }},
    archer = {frame = 192, pivotY = 0.71, clips = {
        idle = {file = 'archer/archer_idle', fps = 9},
        run = {file = 'archer/archer_run', fps = 10},
        attack = {file = 'archer/archer_shoot', fps = 16, loop = false},
    }},
    lancer = {frame = 320, pivotY = 0.62, clips = {
        idle = {file = 'lancer/lancer_idle', fps = 12},
        run = {file = 'lancer/lancer_run', fps = 12},
        attack = {file = 'lancer/lancer_right_attack', fps = 12, loop = false},
        attackUp = {file = 'lancer/lancer_up_attack', fps = 12, loop = false},
        attackDown = {file = 'lancer/lancer_down_attack', fps = 12, loop = false},
    }},
    monk = {frame = 192, pivotY = 0.71, clips = {
        idle = {file = 'monk/idle', fps = 9},
        run = {file = 'monk/run', fps = 10},
        attack = {file = 'monk/heal', fps = 20, loop = false},
    }},
    pawn = {frame = 192, pivotY = 0.71, clips = {
        idle = {file = 'pawn/pawn_idle_axe', fps = 10},
        run = {file = 'pawn/pawn_run_axe', fps = 12},
        attack = {file = 'pawn/pawn_interact_axe', fps = 16, loop = false},
        idleCarry = {file = 'pawn/pawn_idle_wood', fps = 10},
        runCarry = {file = 'pawn/pawn_run_wood', fps = 12},
    }},
}

local cache = {}

function art.texture(path)
    return assets.texture(root .. path)
end

-- Returns the path of the portrait of a class, for the UI.
function art.avatar(class)
    return string.format('%sui/human_avatars/avatars_%02d.png', root, class.avatar)
end

function art.strip(path, frame, options)
    local texture = art.texture(path)
    return animation2d.grid(texture, {frameWidth = frame, frameHeight = options.height or frame, fps = options.fps or 10, loop = options.loop == false and 'once' or 'loop'})
end

-- Returns the clips of a unit kind in a team color, with the pivot that puts the feet of the unit on its position.
function art.unit(kind, color)
    local key = kind .. ':' .. color
    if cache[key] then
        return cache[key]
    end

    local spec = units[kind]
    local clips = {}
    for name, clip in pairs(spec.clips) do
        clips[name] = art.strip('units/' .. color .. '/' .. clip.file .. '.png', spec.frame, clip)
    end
    cache[key] = {clips = clips, pivotY = spec.pivotY}
    return cache[key]
end

-- The animator puts the feet of the unit on the sprite position whenever it applies a frame.
function art.newAnimator(kind, color)
    local unit = art.unit(kind, color)
    local animator = animation2d.newAnimator()
    animator.pivotY = unit.pivotY
    for name, clip in pairs(unit.clips) do
        animator:add(name, clip)
    end
    animator:play('idle')
    return animator, unit
end

return art
