-- The four playable classes. The attack and the special name the moves of `entities/player.lua`, and the icons show them in the HUD.
return {
    {id = 'warrior', health = 140, speed = 300, damage = 30, range = 125, cooldown = 0.55, chop = 1.0, capacity = 6, attack = 'slash', special = 'guard', specialCooldown = 0, attackIcon = 'sword', specialIcon = 'shield'},
    {id = 'archer', health = 90, speed = 330, damage = 22, range = 700, cooldown = 0.6, chop = 0.6, capacity = 5, attack = 'arrow', special = 'volley', specialCooldown = 4, attackIcon = 'bow', specialIcon = 'bolt'},
    {id = 'lancer', health = 120, speed = 310, damage = 34, range = 185, cooldown = 0.75, chop = 0.8, capacity = 6, attack = 'thrust', special = 'charge', specialCooldown = 3, attackIcon = 'spear', specialIcon = 'bolt'},
    {id = 'mage', health = 90, speed = 300, damage = 28, range = 620, cooldown = 1.1, chop = 0.5, capacity = 4, attack = 'fireball', special = 'nova', specialCooldown = 6, attackIcon = 'staff', specialIcon = 'fire'},
}
