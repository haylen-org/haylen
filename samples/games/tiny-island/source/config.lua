-- Tuning values shared by the systems of the game.
return {
    tile = 64,

    fire = {
        maxFuel = 100,
        startFuel = 70,
        woodFuel = 8,
        dayBurn = 0.1,
        nightBurn = 0.25,
        dailyLoss = 15,
        minRadius = 90,
        maxRadius = 520,
        feedDistance = 170,
    },

    cycle = {dawn = 15, day = 150, dusk = 15, night = 90},

    trees = {spacing = 170, fireClearance = 430, health = 3, regrowDays = 2, woodMin = 2, woodMax = 3},

    enemies = {firstNight = 4, perNight = 3, blackFromNight = 4, spawnInterval = 3.5, aggroRange = 900},

    -- Physics categories as bit flags, so shapes pick what they collide with.
    category = {world = 1, player = 2, enemy = 4, barrier = 16, tree = 32},

    layer = {ground = 1, ring = 5, entities = 6, effects = 7, overlay = 8, clouds = 9},
}
