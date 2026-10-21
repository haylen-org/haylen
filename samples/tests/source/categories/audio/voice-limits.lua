-- Voice limits: a hundred coins collected at once with and without the limit of the coin sound, a line of dialogue whose priority keeps it playing while bursts fill every voice, and a tone that stops and resumes in a fast loop without clicks.
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local VoiceLimits = haylen.class('VoiceLimits', AudioTest)

local kCoins = 100
local kCode = [[
audio.setLimit(coin, {voices = 4, cooldown = 0.03})  -- At most four coin sounds at once.
audio.play(coin, {pitchVariation = 0.1, volume = 0.5})
audio.play(line, {priority = 255, loop = true})  -- Outlasts voices of lower priority.
audio.pause(tone)
audio.resume(tone)  -- Both fade over a few milliseconds, so nothing clicks.]]
-- The coin of the terrain atlas spins through its four frames.
local kCoinFrames = {{2, 398, 128, 128}, {134, 398, 128, 128}, {266, 398, 128, 128}, {398, 398, 128, 128}}
local kGravity = 1400

function VoiceLimits:enter()
    self.coin = sounds.get('audio/effects/wood-pickup.ogg')
    self.burst = sounds.get('audio/effects/footstep-1.ogg')
    self.line = sounds.get('audio/generated/pluck-loop.wav')
    self.tone = sounds.get('audio/generated/hum-loop.wav')
    self.limited, self.flicker, self.flickerTime = true, false, 0
    self.peakVoices, self.lineVoice = 0, nil
    self.coins = {}
    self.time = 0
    self.terrain = assets.texture('tiled/tiles/terrain.png', {filter = 'linear', mipmaps = true})
    self.slime = assets.load('sprites/atlases/slime.json', 'atlas', {filter = 'linear'})
    self:applyLimit()
    -- The line plays as the test opens, so the slime speaks until a burst tries to drown it.
    self:playLine()
    self:frame({
        code = kCode,
        hint = 'Collect a hundred coins with and without the limit and compare the voices, fill every voice while the line plays, and let the tone stop and resume quickly.',
        controls = {
            ui.toggle{id = 'limit', text = 'Limit coins to four voices', checked = true, onChange = function(event)
                self.limited = event.checked
                self:applyLimit()
            end},
            ui.button{id = 'coins', text = 'Collect a hundred coins', variant = 'primary', onClick = function() self:collect() end},
            ui.button{id = 'line', text = 'Play a line with priority 255', onClick = function() self:playLine() end},
            ui.button{id = 'burst', text = 'Fill every voice', onClick = function() self:fill() end},
            ui.toggle{id = 'flicker', text = 'Pause and resume a tone fast', onChange = function(event) self:setFlicker(event.checked) end},
            ui.label{text = 'The tone pauses and resumes every 50 milliseconds without clicks, because each pause and resume fades over a few milliseconds.', color = 'textMuted', font = 'caption'},
        },
    })
end

function VoiceLimits:exit()
    VoiceLimits.super.exit(self)
    self:setFlicker(false)
    audio.setLimit(self.coin, {})
    audio.stopAll(0.1)
end

function VoiceLimits:applyLimit()
    audio.setLimit(self.coin, self.limited and {voices = 4, cooldown = 0.03} or {})
end

function VoiceLimits:collect()
    for index = 1, kCoins do
        audio.play(self.coin, {pitchVariation = 0.1, volume = 0.5})
        local angle = -math.pi / 2 + (index / kCoins - 0.5) * 1.6
        local speed = 600 + (index * 37 % 23) * 22
        self.coins[#self.coins + 1] = {x = 0, y = 0, vx = math.cos(angle) * speed, vy = math.sin(angle) * speed, life = 1.2 + (index % 7) * 0.05, spin = index}
    end
end

function VoiceLimits:playLine()
    if self.lineVoice then
        audio.stop(self.lineVoice)
    end
    self.lineVoice = audio.play(self.line, {priority = 255, loop = true, volume = 0.6})
end

function VoiceLimits:fill()
    for _ = 1, 140 do
        audio.play(self.burst, {volume = 0.2, priority = 64})
    end
end

function VoiceLimits:setFlicker(enabled)
    self.flicker = enabled
    if enabled and not self.toneVoice then
        self.toneVoice = audio.play(self.tone, {loop = true, volume = 0.5})
    elseif not enabled and self.toneVoice then
        audio.stop(self.toneVoice)
        self.toneVoice = nil
    end
end

function VoiceLimits:update(dt)
    VoiceLimits.super.update(self, dt)
    if self.flicker and self.toneVoice then
        self.flickerTime = self.flickerTime + dt
        if self.flickerTime >= 0.05 then
            self.flickerTime = 0
            if audio.paused(self.toneVoice) then
                audio.resume(self.toneVoice)
            else
                audio.pause(self.toneVoice)
            end
        end
    end
    self.peakVoices = math.max(self.peakVoices * 0.98, audio.voiceCount())
    self.time = self.time + dt
    for index = #self.coins, 1, -1 do
        local coin = self.coins[index]
        coin.vy = coin.vy + kGravity * dt
        coin.x, coin.y, coin.life = coin.x + coin.vx * dt, coin.y + coin.vy * dt, coin.life - dt
        if coin.life <= 0 then
            table.remove(self.coins, index)
        end
    end
    local line = self.lineVoice and audio.active(self.lineVoice) and 'plays' or 'is silent'
    self:status(string.format('%d voices play, the line %s and the coin limit is %s', audio.voiceCount(), line, self.limited and 'on' or 'off'))
end

function VoiceLimits:draw(area)
    -- The meter of the voices, with its legend under it.
    local left, width = 40, area.width - 80
    Test.caption('Voices that play now', left, 28, {size = 26, color = Test.ink})
    Test.caption(string.format('%d of 128', audio.voiceCount()), left + width, 28, {size = 26, color = Test.ink, anchor = {1, 0}})
    AudioTest.bar(left, 70, width, 26, audio.voiceCount() / 128, Test.accent)
    AudioTest.bar(left, 106, width, 10, self.peakVoices / 128, Test.warm)
    graphics2d.drawShape({left, 136, 28, 12}, {radius = 6, color = Test.accent})
    Test.caption('Voices now', left + 40, 130, {size = 20})
    graphics2d.drawShape({left + 200, 138, 28, 8}, {radius = 4, color = Test.warm})
    Test.caption(string.format('Recent peak, %d voices', math.floor(self.peakVoices)), left + 240, 130, {size = 20})

    -- The coins burst from a chest on the left, and the slime on the right says the line while it plays.
    local ground = area.height - 56
    local chestX = area.width * 0.3
    graphics2d.drawEllipse(chestX, ground, 90, 16, '#50000000')
    graphics2d.draw(self.terrain, chestX, ground + 10, {source = {662, 266, 128, 128}, width = 200, height = 200, pivotY = 1, layer = 1})
    local options = {source = nil, width = 44, height = 44, layer = 2}
    for _, coin in ipairs(self.coins) do
        options.source = kCoinFrames[math.floor(self.time * 12 + coin.spin) % 4 + 1]
        graphics2d.draw(self.terrain, chestX + coin.x, ground - 90 + coin.y, options)
    end
    Test.caption(self.limited and 'A hundred coins, four sounds at most' or 'A hundred coins, a sound for each', chestX, ground + 4, {size = 20, anchor = {0.5, 0}, color = Test.ink})

    local playing = self.lineVoice ~= nil and audio.active(self.lineVoice)
    local slimeX = area.width * 0.62
    local hop = playing and math.abs(math.sin(self.time * 6)) * 10 or 0
    graphics2d.drawEllipse(slimeX, ground, 70, 14, '#50000000')
    graphics2d.draw(self.slime.texture, slimeX, ground - hop, {source = self.slime:source(playing and 'slime 2' or 'slime 0'), width = 170, height = 170, pivotY = 0.95, layer = 1})
    if playing then
        graphics2d.draw(self.slime.texture, slimeX + 110, ground - 150, {source = self.slime:source('slime 14'), width = 220, height = 180, pivotX = 0, pivotY = 1, layer = 2})
        Test.caption('Priority 255', slimeX + 220, ground - 250, {size = 22, color = '#FF15172A', anchor = {0.5, 0.5}, layer = 3})
    end
    Test.caption(playing and 'The line plays on through every burst' or 'The line is silent', slimeX, ground + 4, {size = 20, anchor = {0.5, 0}, color = Test.ink})
end

return VoiceLimits
