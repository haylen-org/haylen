-- Character: a small platformer hero that reads only the move, jump and dash actions, so the keyboard, the mouse, every gamepad and the touch controls drive it at the same time, with prompts that follow the last device.
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Backdrop = require('categories.animation.backdrop')
local controls = require('categories.input.controls')
local Test = require('harness.test')

local Character = haylen.class('Character', Test)

Character.code = [[
local mx = input.vector('move')  -- Keys, sticks and the touch stick at once.
if input.pressed('jump') then hero.buffer = 0.12 end
if input.released('jump') and hero.vy < 0 then hero.vy = hero.vy * 0.45 end  -- A short press jumps lower.
if input.pressed('dash') and hero.cooldown <= 0 then hero.dash = 0.18 end
local prompt = prompts[input.lastDevice()]  -- The value is `keyboardMouse`, `gamepad` or `touch`.]]

local kWidth, kHeight = 56, 104
-- The walker sheet has cells of 256 pixels in eight columns: the idle frames in the first row, the run in the second, and a frame of the run with the legs apart for the air.
local kIdleFrames = {0, 1, 2, 3}
local kRunFrames = {8, 9, 10, 11, 12, 13, 14, 15}
local kAir = 10
local kFeet = 248 / 256
local kHeroSize = 150
-- The painted platform is 777 by 241 pixels with its grass at the top line 22, and its rounded ends take 170 pixels on each side.
local kPlatformHeight = 76
local kPlatformTop = 22 / 241
local kGround = 50
local kRun, kAcceleration = 520, 4200
local kGravity, kJump, kCut = 2600, 1080, 0.45
local kCoyote, kBuffer = 0.1, 0.12
local kDashSpeed, kDashTime, kDashCooldown = 1400, 0.18, 0.6
local kPrompts = {
    keyboardMouse = 'A and D or the arrows run, Space or a left click jumps, and Shift or a right click dashes.',
    gamepad = 'The left stick or the d-pad runs, the bottom button jumps and the left one dashes.',
    touch = 'The stick runs, Jump jumps and Dash dashes.',
}

function Character:enter()
    self.trail = collections.newRingBuffer(12)
    self.coins = 0
    self.time = 0
    self.backdrop = Backdrop()
    self.walker = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.coin = assets.texture('physics/coin.png', {filter = 'linear'})
    local platform = assets.texture('input/platform.png', {filter = 'linear'})
    self.platform = graphics2d.newNineSlice(platform, {borders = {170, 0, 170, 0}})
    self:frame({
        code = Character.code,
        hint = 'Run, jump and dash with any device and collect the coins.',
        play = true,
        controls = {
            ui.formField{label = 'Touch controls', ui.toggle{id = 'touchOnly', text = 'Only after a touch', checked = true, onChange = function(event)
                for _, id in ipairs({'stick', 'jump', 'dash'}) do
                    self:set(id, {touchOnly = event.checked})
                end
            end}},
            ui.button{id = 'respawn', text = 'Put the hero back', onClick = function() self:respawn() end},
            ui.label{text = 'Holding jump jumps higher, a jump pressed just before landing still counts, and a jump just after running off a ledge too.', color = 'textMuted', font = 'caption'},
        },
        overlay = {
            ui.touchStick{id = 'stick', action = 'move', mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
            ui.row{anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}, gap = 32,
                ui.touchButton{id = 'dash', action = 'dash', text = 'Dash', touchOnly = true},
                ui.touchButton{id = 'jump', action = 'jump', text = 'Jump', touchOnly = true},
            },
        },
    })
    self:loadActions(controls.actions())
end

function Character:exit()
    Character.super.exit(self)
    input.clearVirtual()
end

function Character:respawn()
    self.hero = {x = self.area.width * 0.18, y = self.area.height - 260, vx = 0, vy = 0, facing = 1, grounded = false, coyote = 0, buffer = 0, dash = 0, cooldown = 0}
end

function Character:resize(area)
    local w, h = area.width, area.height
    self.platforms = {
        {-200, h - kGround, w + 400, kGround},
        {w * 0.08, h - 220, w * 0.22, 24},
        {w * 0.4, h - 340, w * 0.2, 24},
        {w * 0.7, h - 230, w * 0.22, 24},
        {w * 0.26, h - 460, w * 0.14, 24},
        {w * 0.62, h - 480, w * 0.16, 24},
    }
    self.spots = {}
    for index = 2, #self.platforms do
        local platform = self.platforms[index]
        self.spots[#self.spots + 1] = {x = platform[1] + platform[3] / 2, y = platform[2] - 60, taken = false}
    end
    self:respawn()
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
        self:respawn()
    end
end

function Character:update(dt)
    Character.super.update(self, dt)
    if not self.area then
        return
    end
    self:step(math.min(dt, 1 / 30))
    self.time = self.time + dt
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
    self:status(string.format('The hero is %s at %+.0f, %+.0f units a second, the dash is %s, and %d coins are collected.', hero.grounded and 'on the ground' or 'in the air', hero.vx, hero.vy, hero.cooldown <= 0 and 'ready' or 'cooling down', self.coins))
end

-- Picks the frame of the walker sheet for the state of the hero.
function Character:heroFrame(hero)
    if not hero.grounded then
        return kAir
    end
    if math.abs(hero.vx) > 40 then
        return kRunFrames[math.floor(self.time * 14) % #kRunFrames + 1]
    end
    return kIdleFrames[math.floor(self.time * 6) % #kIdleFrames + 1]
end

function Character:drawHero(x, y, frame, facing, color, layer)
    graphics2d.draw(self.walker, x, y, {source = {frame % 8 * 256, frame // 8 * 256, 256, 256}, width = kHeroSize, height = kHeroSize, pivotY = kFeet, flipHorizontal = facing < 0, color = color, layer = layer})
end

function Character:draw(area)
    self.backdrop:draw(graphics2d.canvasBounds(), area.height - kGround + 6, 150, 0)
    for index = 2, #self.platforms do
        local platform = self.platforms[index]
        local top = platform[2] - kPlatformHeight * kPlatformTop
        graphics2d.drawNineSlice(self.platform, {platform[1] - 8, top, platform[3] + 16, kPlatformHeight}, nil, {borderScale = kPlatformHeight / 241, layer = 1})
    end
    for _, spot in ipairs(self.spots) do
        if not spot.taken then
            local bob = math.sin(haylen.elapsed() * 3 + spot.x) * 6
            local spin = math.abs(math.cos(haylen.elapsed() * 2.5 + spot.x * 0.01))
            graphics2d.draw(self.coin, spot.x, spot.y + bob, {width = 52 * math.max(0.15, spin), height = 52, layer = 2})
        end
    end

    local hero = self.hero
    local frame = self:heroFrame(hero)
    for _, point in ipairs(self.trail:values()) do
        if point[3] then
            self:drawHero(point[1], point[2] + kHeight / 2, kRunFrames[3], hero.facing, '#55FFC93C', 3)
        end
    end
    Backdrop.shadow(hero.x, hero.grounded and hero.y or area.height - kGround + 6, hero.grounded and 90 or 60, 3)
    self:drawHero(hero.x, hero.y, frame, hero.facing, hero.dash > 0 and '#FFFFE9A8' or nil, 4)

    local text = kPrompts[input.lastDevice()]
    local width = graphics2d.measureText(nil, text, {size = 24}) + 48
    graphics2d.drawShape({area.width / 2 - width / 2, 20, width, 48}, {radius = 24, color = '#CC151A2A', layer = 5})
    graphics2d.drawText(nil, text, area.width / 2, 44, {size = 24, color = Test.ink, anchor = {0.5, 0.5}, layer = 6})
end

return Character
