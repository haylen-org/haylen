-- Enemy kinds by team color. Black units arrive from the fourth night and hit harder.
local kinds = {
    warrior = {unit = 'warrior', health = 60, speed = 190, damage = 12, range = 90, cooldown = 1.1, keepDistance = 0},
    archer = {unit = 'archer', health = 40, speed = 200, damage = 9, range = 520, cooldown = 1.8, keepDistance = 380},
    lancer = {unit = 'lancer', health = 110, speed = 160, damage = 16, range = 130, cooldown = 1.4, keepDistance = 0},
}

local colors = {
    red = {health = 1.0, damage = 1.0},
    black = {health = 1.6, damage = 1.4},
}

return {kinds = kinds, colors = colors}
