-- Positional audio: two campfires, a woodcutter and a hum that circles the middle play in a world around a listener that follows the camera, with the fade model, the distances, the rolloff, the pan distance and the Doppler effect, and the volume, pan and pitch each sound gets drawn next to it.
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local particles2d = require('haylen.particles2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local Positional = haylen.class('Positional', AudioTest)

Positional.code = [[
audio.setSpatialization({model = 'inverse', minDistance = 200, maxDistance = 1500, rolloff = 1})
audio.setSpatialization({panDistance = 900, doppler = 1})  -- Keys left out keep their values.
audio.followCamera(camera)  -- Or audio.setListener(x, y).
local fire = audio.play(crackle, {bus = 'ambience', loop = true, x = -760, y = -300})
audio.setPosition(hum, x, y)  -- A moving sound pans and gets the Doppler pitch.]]

Positional.walk = {name = 'walk', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'stick:right', 'virtualStick:walk'}}

local kModels = {{id = 'linear', text = 'Linear'}, {id = 'inverse', text = 'Inverse'}, {id = 'exponential', text = 'Exponential'}}
local kWalk = 520
local kOrbit = 600
-- The camera fits this much of the world around the listener into the play area.
local kSight = {2400, 1500}
-- The frames of the swing of the walker sheet, in cells of 256 pixels on its third row.
local kSwing = {0, 1, 2, 3, 4, 5}
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
    self.x, self.y = 0, 0
    self.orbitSpeed, self.angle = 600, 0
    self.orbitPath = {}
    for step = 0, 63 do
        local angle = step / 64 * math.pi * 2
        self.orbitPath[#self.orbitPath + 1] = math.cos(angle) * kOrbit
        self.orbitPath[#self.orbitPath + 1] = math.sin(angle) * kOrbit
    end
    self.following = true
    audio.setSpatialization({doppler = 1})
    audio.followCamera(self.world)

    self.emitters = {
        {name = 'Campfire', kind = 'fire', x = -760, y = -300, voice = audio.play(sounds.get('audio/ambient/fire-loop.ogg'), {bus = 'ambience', loop = true, x = -760, y = -300})},
        {name = 'Campfire', kind = 'fire', x = 820, y = 380, voice = audio.play(sounds.get('audio/ambient/fire-loop.ogg'), {bus = 'ambience', loop = true, x = 820, y = 380, startAt = 0.4})},
        {name = 'Woodcutter', kind = 'cutter', x = 320, y = -420},
        {name = 'Hum', kind = 'hum', x = kOrbit, y = 0, voice = audio.play(sounds.get('audio/generated/hum-loop.wav'), {loop = true, volume = 0.5, x = kOrbit, y = 0})},
    }
    local linear = {filter = 'linear'}
    self.art = {
        firewood = assets.texture('effects/campfire.png', linear),
        tree = assets.texture('sprites/images/tree.png', linear),
        walker = assets.texture('sprites/sheets/walker.png', linear),
        knight = assets.texture('sprites/images/hero.png', linear),
        orb = assets.texture('sprites/images/bullets.png', linear),
    }
    self.flames = {}
    for index = 1, 2 do
        local flame = particles2d.newSystem(assets.load('particles/library/fire/campfire-realistic.particles'), {seed = index})
        flame.position, flame.scale = {self.emitters[index].x, self.emitters[index].y - 50}, 2.4
        self.flames[index] = flame
    end
    local cutter = self.emitters[3]
    timer.every(1.4, function()
        audio.play(sounds.get('audio/effects/chop.ogg'), {x = cutter.x, y = cutter.y, pitchVariation = 0.08})
        cutter.swing = 0
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
        hint = 'Walk with the arrows, WASD, a stick or the directional pad, with the touch stick, or click where to go.',
        code = Positional.code,
        play = true,
        controls = controls,
        overlay = {ui.touchStick{id = 'stick', action = 'walk', mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300}},
    })
    self:loadActions({actions = {Positional.walk}})
end

function Positional:exit()
    Positional.super.exit(self)
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

-- The pitch the Doppler effect gives a moving sound, from the movement of the sound and the listener since the last frame along the line between them, within two octaves.
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
    local zoom = math.min(self.area.width / kSight[1], self.area.height / kSight[2])
    self.world.zoom = {zoom, zoom}
    for _, flame in ipairs(self.flames) do
        flame:update(dt)
    end

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
    local cutter = self.emitters[3]
    cutter.swing = cutter.swing and cutter.swing + dt

    local lx, ly = audio.listener()
    hum.heard = self:dopplerPitch(hum, lx, ly, dt)
    self:status(string.format('Listener at %.0f, %.0f, the hum plays at a pitch of %.2f and is heard at %.2f times, %d voices', lx, ly, audio.pitch(hum.voice), hum.heard, audio.voiceCount()))
end

function Positional:drawWorld()
    local settings = audio.spatialization()
    local unit = 1 / self.world.zoom.x
    for line = -2000, 2000, 250 do
        graphics2d.drawLine(line, -1500, line, 1500, 2 * unit, '#FF1F2536')
        graphics2d.drawLine(-2000, line * 0.75, 2000, line * 0.75, 2 * unit, '#FF1F2536')
    end
    graphics2d.drawStroke(self.orbitPath, {width = 2 * unit, color = '#50C9A0FF', closed = true, dashes = {18 * unit, 14 * unit}, layer = 1})

    local lx, ly = audio.listener()
    graphics2d.drawRing(lx, ly, settings.minDistance, 3 * unit, Test.green, {layer = 1})
    graphics2d.drawRing(lx, ly, settings.maxDistance, 3 * unit, Test.red, {layer = 1})
    for side = -1, 1, 2 do
        local x = lx + side * settings.panDistance
        graphics2d.drawStroke({x, ly - 140, x, ly + 140}, {width = 3 * unit, color = Test.accent, cap = 'round', layer = 1})
    end

    for _, emitter in ipairs(self.emitters) do
        local volume, pan = self:heard(emitter, lx, ly)
        graphics2d.drawCircle(emitter.x, emitter.y - 60, 70 + 110 * volume, '#24FFFFFF', {layer = 1})
        self:drawSource(emitter)
        local pitch = emitter.heard and string.format(', pitch %.2f', emitter.heard) or ''
        graphics2d.drawText(nil, emitter.name, emitter.x, emitter.y + 26, {size = 26 * unit, anchor = {0.5, 0}, outlineWidth = 3 * unit, outlineColor = '#FF15172A', layer = 6})
        graphics2d.drawText(nil, string.format('Volume %.2f, pan %+.2f%s', volume, pan, pitch), emitter.x, emitter.y + 26 + 32 * unit, {size = 20 * unit, color = '#FFCDD3E6', anchor = {0.5, 0}, outlineWidth = 3 * unit, outlineColor = '#FF15172A', layer = 6})
    end

    graphics2d.drawEllipse(self.x, self.y, 60, 14, '#50000000', {layer = 3})
    graphics2d.draw(self.art.knight, self.x, self.y, {width = 150, height = 150, pivotY = 1, layer = 4})
    local top = ly - 190
    graphics2d.drawPolygon({{lx, top - 30}, {lx + 26, top + 16}, {lx - 26, top + 16}}, '#FFFFFFFF', {layer = 5})
end

-- Each sound draws as what makes it: a campfire with its flames, a woodcutter who swings at a tree, and a humming orb.
function Positional:drawSource(emitter)
    if emitter.kind == 'fire' then
        graphics2d.draw(self.art.firewood, emitter.x, emitter.y, {width = 230, height = 144, pivotY = 1, layer = 2})
    elseif emitter.kind == 'cutter' then
        graphics2d.draw(self.art.tree, emitter.x + 70, emitter.y, {width = 200, height = 300, pivotY = 1, layer = 2})
        local frame = emitter.swing and math.min(#kSwing, math.floor(emitter.swing / 0.08) + 1) or #kSwing
        graphics2d.draw(self.art.walker, emitter.x - 70, emitter.y, {source = {kSwing[frame] * 256, 512, 256, 256}, width = 190, height = 190, pivotY = 1, layer = 3})
    else
        graphics2d.drawCircle(emitter.x, emitter.y - 60, 56, '#40C9A0FF', {layer = 2})
        graphics2d.draw(self.art.orb, emitter.x, emitter.y - 60, {source = {128, 0, 64, 64}, width = 84, height = 84, layer = 3})
    end
end

function Positional:draw(area)
    graphics2d.beginWorld(self.world)
    self:drawWorld()
    for _, flame in ipairs(self.flames) do
        flame:draw()
    end

    -- The legend sits on a plate in the lower corner of the play area, above the world.
    graphics2d.beginWorld(self.camera)
    local settings = audio.spatialization()
    local width = math.min(area.width - 32, 520)
    local top = area.height - 192
    graphics2d.drawShape({16, top, width, 176}, {radius = 16, color = '#E0141824', borderWidth = 2, borderColor = '#FF2C3147', layer = 10})
    local rows = {
        {Test.green, string.format('Full volume within %.0f', settings.minDistance)},
        {Test.red, string.format('%s past %.0f', settings.model == 'linear' and 'Silent' or 'Steady', settings.maxDistance)},
        {Test.accent, string.format('Full pan at %.0f to each side', settings.panDistance)},
        {'#FFFFFFFF', self.following and 'The white triangle is the listener, on the camera' or 'The listener stays where the camera left it'},
    }
    Test.caption(string.format('The "%s" fade, Doppler %.1f', settings.model, settings.doppler), 36, top + 10, {size = 22, color = Test.ink, layer = 11})
    for index, row in ipairs(rows) do
        local y = top + 10 + index * 34
        graphics2d.drawCircle(46, y + 12, 8, row[1], {layer = 11})
        Test.caption(row[2], 66, y, {size = 20, maxWidth = width - 70, layer = 11})
    end
end

return Positional
