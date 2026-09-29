-- Character: a small platformer hero that reads only the move, jump and dash actions, so the keyboard, the mouse, every gamepad and the touch controls drive it at the same time, with prompts that follow the last device.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = require('sample')

local Character = haylen.class('Character', sample.Test)

local kWidth, kHeight = 48, 64
local kRun, kAcceleration = 520, 4200
local kGravity, kJump, kCut = 2600, 1080, 0.45
local kCoyote, kBuffer = 0.1, 0.12
local kDashSpeed, kDashTime, kDashCooldown = 1400, 0.18, 0.6
local kPrompts = {
    keyboardMouse = 'A and D or the arrows run, Space or a left click jumps, Left Shift or a right click dashes.',
    gamepad = 'The left stick or the d-pad runs, south jumps, west dashes.',
    touch = 'The stick runs, Jump jumps and Dash dashes.',
}

function Character:enter()
    self.trail = collections.newRingBuffer(12)
    self.coins = 0
    self:frame({
        hint = 'Run, jump and dash with any device and collect the coins.',
        navigation = not window.hasPointerDevice(),
        focus = 'touchOnly',
        controls = {
            ui.formField{label = 'Touch controls', ui.toggle{id = 'touchOnly', text = 'Only after a touch', checked = true, onChange = function(event)
                for _, id in ipairs({'stick', 'jump', 'dash'}) do
                    self:set(id, {touchOnly = event.checked})
                end
            end}},
            ui.button{id = 'respawn', text = 'Put the hero back', onClick = function() self:spawn() end},
            ui.label{text = 'Holding jump jumps higher, a jump pressed just before landing still counts, and a jump just after running off a ledge too.', color = 'textMuted', font = 'caption'},
            ui.sectionTitle{text = 'All it reads'},
            ui.label{font = 'monospace', text = "local x = input.vector('move')\ninput.pressed('jump')\ninput.released('jump')\ninput.pressed('dash')\ninput.lastDevice()"},
        },
        overlay = {
            ui.touchStick{id = 'stick', action = 'move', floating = true, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
            ui.row{anchor = 'bottomRight', margin = {0, 540, 110, 0}, gap = 32,
                ui.touchButton{id = 'dash', action = 'dash', text = 'Dash', touchOnly = true},
                ui.touchButton{id = 'jump', action = 'jump', text = 'Jump', touchOnly = true},
            },
        },
    })
end

function Character:exit()
    input.clearVirtual()
end

function Character:spawn()
    self.hero = {x = self.area.width * 0.18, y = self.area.height - 200, vx = 0, vy = 0, facing = 1, grounded = false, coyote = 0, buffer = 0, dash = 0, cooldown = 0}
end

function Character:resize(area)
    local w, h = area.width, area.height
    self.platforms = {
        {0, h - 50, w, 50},
        {w * 0.08, h - 220, w * 0.22, 24},
        {w * 0.4, h - 340, w * 0.2, 24},
        {w * 0.7, h - 230, w * 0.22, 24},
        {w * 0.26, h - 470, w * 0.14, 24},
        {w * 0.62, h - 520, w * 0.16, 24},
    }
    self.spots = {}
    for index = 2, #self.platforms do
        local platform = self.platforms[index]
        self.spots[#self.spots + 1] = {x = platform[1] + platform[3] / 2, y = platform[2] - 50, taken = false}
    end
    self:spawn()
end

-- Moves the hero along one axis and pushes it out of every platform it overlaps, returning whether it landed.
function Character:move(axis, amount)
    local hero = self.hero
    hero[axis] = hero[axis] + amount
    local landed = false
    for _, platform in ipairs(self.platforms) do
        local left, top = hero.x - kWidth / 2, hero.y - kHeight
        if left < platform[1] + platform[3] and left + kWidth > platform[1] and top < platform[2] + platform[4] and top + kHeight > platform[2] then
            if axis == 'x' then
                hero.x = amount > 0 and platform[1] - kWidth / 2 or platform[1] + platform[3] + kWidth / 2
                hero.vx = 0
            elseif amount > 0 then
                hero.y, hero.vy, landed = platform[2], 0, true
            else
                hero.y, hero.vy = platform[2] + platform[4] + kHeight, 0
            end
        end
    end
    return landed
end

function Character:step(dt)
    local hero = self.hero
    local mx = input.vector('move')
    hero.buffer = input.pressed('jump') and kBuffer or math.max(0, hero.buffer - dt)
    hero.coyote = hero.grounded and kCoyote or math.max(0, hero.coyote - dt)
    hero.cooldown = math.max(0, hero.cooldown - dt)
    if math.abs(mx) > 0.1 then
        hero.facing = mx > 0 and 1 or -1
    end

    if input.pressed('dash') and hero.cooldown <= 0 then
        hero.dash, hero.cooldown, hero.vy = kDashTime, kDashCooldown, 0
    end
    if hero.dash > 0 then
        hero.dash = hero.dash - dt
        hero.vx = hero.facing * kDashSpeed
    else
        local target = mx * kRun
        local change = kAcceleration * dt
        hero.vx = hero.vx + math.max(-change, math.min(change, target - hero.vx))
        hero.vy = hero.vy + kGravity * dt
    end

    if hero.buffer > 0 and hero.coyote > 0 then
        hero.vy, hero.buffer, hero.coyote = -kJump, 0, 0
    end
    if input.released('jump') and hero.vy < 0 then
        hero.vy = hero.vy * kCut
    end

    self:move('x', hero.vx * dt)
    hero.grounded = self:move('y', hero.vy * dt)
    hero.x = math.max(kWidth / 2, math.min(self.area.width - kWidth / 2, hero.x))
    if hero.y > self.area.height + 200 then
        self:spawn()
    end
end

function Character:update(dt)
    Character.super.update(self, dt)
    if not self.area then
        return
    end
    self:step(math.min(dt, 1 / 30))
    local hero = self.hero
    self.trail:push({hero.x, hero.y - kHeight / 2, hero.dash > 0})

    local left = 0
    for _, spot in ipairs(self.spots) do
        if not spot.taken and math.abs(spot.x - hero.x) < 40 and math.abs(spot.y - (hero.y - kHeight / 2)) < 50 then
            spot.taken = true
            self.coins = self.coins + 1
        end
        left = left + (spot.taken and 0 or 1)
    end
    if left == 0 then
        for _, spot in ipairs(self.spots) do
            spot.taken = false
        end
    end
    self:status(string.format('last device %s   speed %+.0f, %+.0f   grounded %s   dash ready %s   coins %d', input.lastDevice(), hero.vx, hero.vy, hero.grounded, hero.cooldown <= 0, self.coins))
end

function Character:draw(area)
    for _, platform in ipairs(self.platforms) do
        graphics2d.drawRect(platform, sample.surface)
        graphics2d.drawRect({platform[1], platform[2], platform[3], 6}, sample.green, {layer = 1})
    end
    for _, spot in ipairs(self.spots) do
        if not spot.taken then
            local bob = math.sin(haylen.elapsed() * 3 + spot.x) * 6
            graphics2d.drawCircle(spot.x, spot.y + bob, 16, sample.warm, {layer = 1})
        end
    end

    for _, point in ipairs(self.trail:values()) do
        if point[3] then
            graphics2d.drawRect({point[1] - kWidth / 2, point[2] - kHeight / 2, kWidth, kHeight}, '#40F2B23A', {layer = 2})
        end
    end
    local hero = self.hero
    local rect = {hero.x - kWidth / 2, hero.y - kHeight, kWidth, kHeight}
    graphics2d.drawRect(rect, hero.dash > 0 and sample.warm or sample.accent, {layer = 3})
    graphics2d.drawCircle(hero.x + hero.facing * 12, hero.y - kHeight + 22, 6, '#FF101418', {layer = 4})

    sample.caption(kPrompts[input.lastDevice()], area.width / 2, 20, {anchor = {0.5, 0}, size = 24, color = sample.ink})
end

return Character
