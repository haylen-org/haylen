-- Positional audio: two campfires, a woodcutter and a hum that circles the middle play in a world around a listener that follows the camera, with the fade model, the distances, the rolloff, the pan distance and the Doppler effect, and the volume, pan and pitch each sound gets drawn next to it.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local sample = require('sample')
local sounds = require('sounds')

local Positional = haylen.class('Positional', sample.Test)

local kModels = {{id = 'linear', text = 'Linear'}, {id = 'inverse', text = 'Inverse'}, {id = 'exponential', text = 'Exponential'}}
local kWalk = 520
local kOrbit = 700
local kSettings = {
    {key = 'minDistance', label = 'Full volume within', min = 10, max = 600, step = 10},
    {key = 'maxDistance', label = 'Fading stops at', min = 300, max = 3000, step = 50},
    {key = 'rolloff', label = 'Rolloff', min = 0.1, max = 3, step = 0.1},
    {key = 'panDistance', label = 'Pan distance', min = 100, max = 3000, step = 50},
    {key = 'doppler', label = 'Doppler factor', min = 0, max = 3, step = 0.1},
    {key = 'speedOfSound', label = 'Speed of sound', min = 500, max = 6000, step = 100},
}

function Positional:enter()
    self.defaults = audio.spatialization()
    self.world = graphics2d.newCamera()
    self.world.zoom = {0.45, 0.45}
    self.x, self.y = 0, 0
    self.orbitSpeed, self.angle = 600, 0
    self.following = true
    audio.setSpatialization({doppler = 1})
    audio.followCamera(self.world)

    self.emitters = {
        {name = 'Campfire', x = -700, y = -350, color = sample.warm, voice = audio.play(sounds.get('audio/ambient/fire_loop.ogg'), {bus = 'ambience', loop = true, x = -700, y = -350})},
        {name = 'Campfire', x = 950, y = 450, color = sample.warm, voice = audio.play(sounds.get('audio/ambient/fire_loop.ogg'), {bus = 'ambience', loop = true, x = 950, y = 450, startAt = 0.4})},
        {name = 'Woodcutter', x = 250, y = -800, color = sample.green},
        {name = 'Hum', x = kOrbit, y = 0, color = sample.violet, voice = audio.play(sounds.get('audio/generated/hum_loop.wav'), {loop = true, volume = 0.5, x = kOrbit, y = 0})},
    }
    local cutter = self.emitters[3]
    timer.every(1.4, function()
        audio.play(sounds.get('audio/effects/chop.ogg'), {x = cutter.x, y = cutter.y, pitchVariation = 0.08})
        cutter.flash = 0.2
    end, {owner = self})

    local settings = audio.spatialization()
    local controls = {
        ui.formField{label = 'Fade model', ui.segmentedControl{id = 'model', items = kModels, selected = settings.model, onChange = function(event) self:apply({model = event.value}) end}},
    }
    for _, setting in ipairs(kSettings) do
        controls[#controls + 1] = ui.formField{label = setting.label, ui.slider{id = setting.key, min = setting.min, max = setting.max, step = setting.step, value = settings[setting.key], showValue = true, decimals = setting.step < 1 and 1 or 0, onChange = function(event)
            self:apply({[setting.key] = event.value})
        end}}
    end
    controls[#controls + 1] = ui.formField{label = 'Speed of the hum', ui.slider{id = 'orbit', min = 0, max = 1500, step = 50, value = self.orbitSpeed, showValue = true, decimals = 0, onChange = function(event) self.orbitSpeed = event.value end}}
    controls[#controls + 1] = ui.toggle{id = 'follow', text = 'Listener on the camera', checked = true, onChange = function(event)
        self.following = event.checked
        audio.followCamera(event.checked and self.world or nil)
    end}
    self:frame({
        hint = 'Walk with WASD, the right stick or the touch stick, or click where to go.',
        focus = 'model',
        controls = controls,
        overlay = {ui.touchStick{id = 'stick', action = 'walk', floating = true, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300}},
    })
end

function Positional:exit()
    for _, emitter in ipairs(self.emitters) do
        if emitter.voice then
            audio.stop(emitter.voice, 0.3)
        end
    end
    audio.followCamera(nil)
    audio.setListener(0, 0)
    audio.setSpatialization(self.defaults)
    input.clearVirtual()
end

-- The distances must keep the minimum below the maximum, and the inverse and exponential models need a minimum above 0, which the sliders already keep.
function Positional:apply(changes)
    local settings = audio.spatialization()
    for key, value in pairs(changes) do
        settings[key] = value
    end
    settings.maxDistance = math.max(settings.maxDistance, settings.minDistance + 10)
    audio.setSpatialization(settings)
end

-- The volume and pan a positional voice gets from the listener, with the formulas of the fade models.
function Positional:heard(emitter, listenerX, listenerY)
    local settings = audio.spatialization()
    local distance = math.max(settings.minDistance, math.min(settings.maxDistance, math.sqrt((emitter.x - listenerX) ^ 2 + (emitter.y - listenerY) ^ 2)))
    local volume
    if settings.model == 'linear' then
        volume = 1 - settings.rolloff * (distance - settings.minDistance) / (settings.maxDistance - settings.minDistance)
    elseif settings.model == 'inverse' then
        volume = settings.minDistance / (settings.minDistance + settings.rolloff * (distance - settings.minDistance))
    else
        volume = (distance / settings.minDistance) ^ -settings.rolloff
    end
    local pan = math.max(-1, math.min(1, (emitter.x - listenerX) / settings.panDistance))
    return math.max(0, math.min(1, volume)), pan
end

-- The pitch the Doppler effect gives a moving sound, by the formula of OpenAL from the movement of the sound and the listener since the last frame, within two octaves.
function Positional:dopplerPitch(emitter, lx, ly, dt)
    local settings = audio.spatialization()
    local previous = self.previous or {emitter.x, emitter.y, lx, ly}
    self.previous = {emitter.x, emitter.y, lx, ly}
    local dx, dy = lx - emitter.x, ly - emitter.y
    local distance = math.sqrt(dx * dx + dy * dy)
    if settings.doppler == 0 or distance < 1 or dt <= 0 then
        return 1
    end
    local listenerSpeed = ((lx - previous[3]) * dx + (ly - previous[4]) * dy) / distance / dt
    local sourceSpeed = ((emitter.x - previous[1]) * dx + (emitter.y - previous[2]) * dy) / distance / dt
    local shift = (settings.speedOfSound - settings.doppler * listenerSpeed) / (settings.speedOfSound - settings.doppler * sourceSpeed)
    return math.max(0.25, math.min(4, shift))
end

function Positional:update(dt)
    Positional.super.update(self, dt)
    if not self.area then
        return
    end
    self.world.viewport = self.camera.viewport

    local wx, wy = input.vector('walk')
    if input.mousePressed('left') and not ui.usingPointer() then
        self.targetX, self.targetY = self.world:screenToWorld(input.mousePosition())
    end
    if wx ~= 0 or wy ~= 0 then
        self.targetX = nil
    elseif self.targetX then
        local dx, dy = self.targetX - self.x, self.targetY - self.y
        local length = math.sqrt(dx * dx + dy * dy)
        if length < 10 then
            self.targetX = nil
        else
            wx, wy = dx / length, dy / length
        end
    end
    self.x = math.max(-1800, math.min(1800, self.x + wx * kWalk * dt))
    self.y = math.max(-1300, math.min(1300, self.y + wy * kWalk * dt))
    self.world.x, self.world.y = self.x, self.y

    local hum = self.emitters[4]
    self.angle = self.angle + self.orbitSpeed / kOrbit * dt
    hum.x, hum.y = math.cos(self.angle) * kOrbit, math.sin(self.angle) * kOrbit
    audio.setPosition(hum.voice, hum.x, hum.y)
    self.emitters[3].flash = math.max(0, (self.emitters[3].flash or 0) - dt)

    local lx, ly = audio.listener()
    hum.heard = self:dopplerPitch(hum, lx, ly, dt)
    self:status(string.format('listener %.0f, %.0f   camera %.0f, %.0f   hum pitch set %.2f, heard x%.2f   voices %d', lx, ly, self.world.x, self.world.y, audio.pitch(hum.voice), hum.heard, audio.voiceCount()))
end

function Positional:drawWorld()
    local settings = audio.spatialization()
    for line = -2000, 2000, 250 do
        graphics2d.drawLine(line, -1500, line, 1500, 4, '#FF20263A')
        graphics2d.drawLine(-2000, line * 0.75, 2000, line * 0.75, 4, '#FF20263A')
    end
    graphics2d.drawRing(0, 0, kOrbit, 4, '#40C9A0FF', {layer = 1})

    local lx, ly = audio.listener()
    graphics2d.drawRing(lx, ly, settings.minDistance, 6, sample.green, {layer = 1})
    graphics2d.drawRing(lx, ly, settings.maxDistance, 6, sample.red, {layer = 1})
    graphics2d.drawLine(lx - settings.panDistance, ly - 120, lx - settings.panDistance, ly + 120, 6, sample.accent, {layer = 1})
    graphics2d.drawLine(lx + settings.panDistance, ly - 120, lx + settings.panDistance, ly + 120, 6, sample.accent, {layer = 1})

    for _, emitter in ipairs(self.emitters) do
        local volume, pan = self:heard(emitter, lx, ly)
        local radius = 44 + (emitter.flash or 0) * 90
        graphics2d.drawCircle(emitter.x, emitter.y, radius, emitter.color, {layer = 2})
        graphics2d.drawCircle(emitter.x, emitter.y, radius + 60 * volume, '#30FFFFFF', {layer = 1})
        local pitch = emitter.heard and string.format('\nDoppler pitch x%.2f', emitter.heard) or ''
        sample.caption(string.format('%s\nvolume %.2f  pan %+.2f%s', emitter.name, volume, pan, pitch), emitter.x, emitter.y + 80, {anchor = {0.5, 0}, color = sample.ink, size = 40, layer = 3})
    end

    graphics2d.drawCircle(self.x, self.y, 40, sample.accent, {layer = 4})
    graphics2d.drawPolygon({{lx, ly - 34}, {lx + 30, ly + 22}, {lx - 30, ly + 22}}, sample.ink, {layer = 5})
end

function Positional:draw(area)
    graphics2d.beginWorld(self.world)
    self:drawWorld()

    graphics2d.beginWorld(self.camera)
    local settings = audio.spatialization()
    sample.caption(string.format('%s fade, full volume within %.0f (green), silent or steady past %.0f (red), full pan at %.0f (blue), Doppler %.1f', settings.model, settings.minDistance, settings.maxDistance, settings.panDistance, settings.doppler), 20, 16, {size = 20, color = sample.ink, maxWidth = area.width - 40})
    sample.caption(self.following and 'The white triangle is the listener, on the camera.' or 'The listener stays where the camera left it.', 20, area.height - 40, {size = 20})
end

return Positional
