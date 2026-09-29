-- Tuning values of the quest. Sizes are design units, speeds are units per second and times are seconds.
return {
    -- Design units per pixel of the art, which draws with nearest filtering.
    pixel = 3,
    groundHeight = 36,

    hero = {health = 10, speed = 70, reach = 58, attackInterval = 0.75, restTime = 3, marginLeft = 90, marginRight = 40},

    enemies = {
        slime = {health = 3, damage = 1, speed = 34, interval = 1.3, coins = 2, width = 48, height = 36, burst = 'goo'},
        mushroom = {health = 7, damage = 2, speed = 22, interval = 1.6, coins = 4, width = 48, height = 42, burst = 'spores'},
    },

    spawn = {firstDelay = 1, minDelay = 1.5, maxDelay = 3.5, minAhead = 240, maxAhead = 440, alive = 3, mushroomFrom = 5, mushroomChance = 0.35},

    shop = {sword = 5, potion = 4, heal = 5},

    -- The size of the window mode in desktop points, and a smaller design size that draws the quest larger in it.
    window = {width = 1280, height = 400, designWidth = 1024, designHeight = 320},

    layer = {backdrop = 0, ground = 1, props = 2, enemies = 3, hero = 4, coins = 5, effects = 6, text = 7},
}
