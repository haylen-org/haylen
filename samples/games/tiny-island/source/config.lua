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

    trees = {spacing = 230, fireClearance = 430, health = 3, regrowDays = 2, woodMin = 2, woodMax = 3},

    -- Food runs down over time, and a survivor with an empty belly loses health until it eats.
    food = {max = 100, start = 80, hunger = 0.4, meal = 35, mealHealth = 8, starving = 2},

    sheep = {herd = 6, health = 40, speed = 70, fleeSpeed = 270, meat = 2, spacing = 260},

    enemies = {firstNight = 4, perNight = 3, spawnInterval = 3.2, aggroRange = 900, growth = 0.12},

    -- Physics categories as bit flags, so shapes pick what they collide with.
    category = {world = 1, player = 2, enemy = 4, sheep = 8, barrier = 16, tree = 32},

    layer = {ground = 1, ring = 5, entities = 6, effects = 7, overlay = 8, clouds = 9},
}
