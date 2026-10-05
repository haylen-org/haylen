-- The raiders that land at night. Each kind joins the waves from its first night and fights its own way: imps weave in fast, brutes wind up a smash, throwers keep away and throw javelins, and bombers lob bombs that burst where they land.
return {
    imp = {health = 26, speed = 330, damage = 6, range = 75, cooldown = 0.7, keepDistance = 0, weave = 70, firstNight = 1, weight = 3},
    brute = {health = 150, speed = 140, damage = 22, range = 135, cooldown = 1.9, keepDistance = 0, windup = 0.4, smash = 120, firstNight = 1, weight = 2},
    thrower = {health = 55, speed = 200, damage = 12, range = 560, cooldown = 1.9, keepDistance = 380, release = 0.25, firstNight = 2, weight = 2},
    bomber = {health = 65, speed = 175, damage = 22, range = 430, cooldown = 2.6, keepDistance = 280, release = 0.25, blast = 110, firstNight = 3, weight = 1.5},
}
