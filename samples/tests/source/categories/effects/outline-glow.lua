-- Outline and glow: the `effect` of a draw traces an outline and a soft glow around the visible pixels of a sprite on the GPU, so the sprite the cursor points at shows a selection outline and the sprites picked with a press glow, with no outlined copy of any image.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Scenery = require('categories.effects.scenery')

local OutlineGlow = haylen.class('OutlineGlow', ParticleTest)

OutlineGlow.code = [[
graphics2d.draw(texture, x, y, {pivotY = 1, effect = {outlineWidth = 8, outlineColor = '#FFFFC830'}})
graphics2d.draw(texture, x, y, {pivotY = 1, effect = {glowSize = 36, glowColor = '#E0FFC040'}})]]

OutlineGlow.subjects = {
    {name = 'Knight', path = 'sprites/images/hero.png', x = -720, scale = 1.3, glowing = true},
    {name = 'Bunny', path = 'sprites/images/bunny.png', x = -360, scale = 2.5},
    {name = 'Tree', path = 'sprites/images/tree.png', x = 0, scale = 1.6},
    {name = 'Rock', path = 'sprites/images/rock.png', x = 360, scale = 2.25, glowing = true},
    {name = 'Chest', path = 'tiled/tiles/terrain.png', source = {662, 266, 128, 128}, x = 720, scale = 1.5, pivotY = 0.92},
}
OutlineGlow.outlineColors = {
    {id = 'white', text = 'White', color = '#FFFFFFFF'},
    {id = 'gold', text = 'Gold', color = '#FFFFC830'},
    {id = 'cyan', text = 'Cyan', color = '#FF40E8FF'},
    {id = 'red', text = 'Red', color = '#FFFF4040'},
}
OutlineGlow.glowColors = {
    {id = 'gold', text = 'Gold', color = '#E0FFC040'},
    {id = 'cyan', text = 'Cyan', color = '#E040E0FF'},
    {id = 'violet', text = 'Violet', color = '#E0C060FF'},
    {id = 'green', text = 'Green', color = '#E060FF80'},
}
OutlineGlow.reach = 260

function OutlineGlow.items(colors)
    local items = {}
    for index, color in ipairs(colors) do
        items[index] = {id = color.id, text = color.text}
    end
    return items
end

function OutlineGlow.colorOf(colors, id)
    for _, color in ipairs(colors) do
        if color.id == id then
            return color.color
        end
    end
end

function OutlineGlow:init(entry)
    OutlineGlow.super.init(self, entry)
    self.scenery = Scenery()
    self.time = 0
    self.night = true
    self.pulse = true
    self.outlineAll = false
    self.outlineWidth, self.glowSize = 8, 36
    self.outlineColor = OutlineGlow.outlineColors[1].color
    self.glowColor = OutlineGlow.glowColors[1].color
    self.subjectStates = {}
    for index, subject in ipairs(OutlineGlow.subjects) do
        self.subjectStates[index] = {
            subject = subject,
            texture = assets.texture(subject.path, {filter = 'linear'}),
            glowing = subject.glowing == true,
            options = {source = subject.source, scaleX = subject.scale, scaleY = subject.scale, pivotY = subject.pivotY or 1, layer = 2, effect = {}},
        }
    end
end

function OutlineGlow:enter()
    self:frame{
        hint = 'The sprite nearest the cursor shows its outline. A click, a tap, E, Enter, Space or the south button makes it glow or stop glowing.',
        code = OutlineGlow.code,
        cursor = true,
        controls = {
            ui.toggle{id = 'night', text = 'Night', checked = self.night, onChange = function(event) self.night = event.checked end},
            ui.toggle{id = 'pulse', text = 'Pulse the glow', checked = self.pulse, onChange = function(event) self.pulse = event.checked end},
            ui.toggle{id = 'all', text = 'Outline every sprite', checked = self.outlineAll, onChange = function(event) self.outlineAll = event.checked end},
            ui.formField{label = 'Outline width in pixels', ui.slider{id = 'outlineWidth', min = 1, max = 24, value = self.outlineWidth, showValue = true, decimals = 1, onChange = function(event)
                self.outlineWidth = event.value
            end}},
            ui.formField{label = 'Outline color', ui.combo{id = 'outlineColor', items = OutlineGlow.items(OutlineGlow.outlineColors), selected = OutlineGlow.outlineColors[1].id, onChange = function(event)
                self.outlineColor = OutlineGlow.colorOf(OutlineGlow.outlineColors, event.value)
            end}},
            ui.formField{label = 'Glow size in pixels', ui.slider{id = 'glowSize', min = 2, max = 64, value = self.glowSize, showValue = true, decimals = 0, onChange = function(event)
                self.glowSize = event.value
            end}},
            ui.formField{label = 'Glow color', ui.combo{id = 'glowColor', items = OutlineGlow.items(OutlineGlow.glowColors), selected = OutlineGlow.glowColors[1].id, onChange = function(event)
                self.glowColor = OutlineGlow.colorOf(OutlineGlow.glowColors, event.value)
            end}},
        },
    }
end

-- Returns the sprite whose middle is nearest the cursor, when the cursor is close enough to it.
function OutlineGlow:pointed()
    local best, distance = nil, OutlineGlow.reach
    for _, state in ipairs(self.subjectStates) do
        local subject = state.subject
        local middle = Scenery.ground - (subject.source and subject.source[4] or state.texture.height) * subject.scale / 2
        local away = math.sqrt((subject.x - self.cursorX) ^ 2 + (middle - self.cursorY) ^ 2)
        if away < distance then
            best, distance = state, away
        end
    end
    return best
end

function OutlineGlow:update(dt)
    OutlineGlow.super.update(self, dt)
    self.scenery:update(dt)
    self.time = self.time + dt
    self.target = self.stage and self:pointed()
    if self.target and self:pressed() then
        self.target.glowing = not self.target.glowing
    end

    local glow = self.pulse and self.glowSize * (0.7 + 0.3 * math.sin(self.time * 5)) or self.glowSize
    local glowing = 0
    for _, state in ipairs(self.subjectStates) do
        local effect = state.options.effect
        local outlined = self.outlineAll or state == self.target
        effect.outlineWidth = outlined and self.outlineWidth or 0
        effect.outlineColor = self.outlineColor
        effect.glowSize = state.glowing and glow or 0
        effect.glowColor = self.glowColor
        if state.glowing then
            glowing = glowing + 1
        end
    end
    self:report('Pointed at %s, %d glowing, outline %.1f px, glow %.1f px', self.target and 'the ' .. self.target.subject.name:lower() or 'nothing', glowing, self.outlineWidth, glow)
end

function OutlineGlow:draw(area)
    self.scenery:draw(self.night)
    for _, state in ipairs(self.subjectStates) do
        local subject = state.subject
        graphics2d.draw(state.texture, subject.x, Scenery.ground - (subject.y or 0), state.options)
        ParticleTest.label(subject.name, subject.x, Scenery.ground + 20)
    end
end

return OutlineGlow
