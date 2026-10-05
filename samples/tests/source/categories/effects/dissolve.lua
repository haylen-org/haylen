-- Dissolve: the `effect` of a draw hides the pixels of a sprite whose value noise falls under `dissolve` and colors a band along the border with `dissolveColor`, all on the GPU, so the sprites dissolve and come back without any texture of their own.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Scenery = require('categories.effects.scenery')

local Dissolve = haylen.class('Dissolve', ParticleTest)

Dissolve.subjects = {
    {name = 'Robot', path = 'sprites/images/hero.png', x = -600, scale = 2.8},
    {name = 'Rock', path = 'sprites/images/rock.png', x = -200, scale = 4.5},
    {name = 'Tree', path = 'sprites/images/tree.png', x = 200, scale = 3.4},
    {name = 'Planet', path = 'shaders/images/planet.png', x = 620, scale = 1.1, y = 200, pivotY = 0.5},
}
Dissolve.edgeColors = {
    {id = 'embers', text = 'Embers', color = '#FFFF9A30'},
    {id = 'magic', text = 'Magic', color = '#FF70E4FF'},
    {id = 'toxic', text = 'Toxic', color = '#FFA4FF40'},
    {id = 'none', text = 'No edge color', color = '#00000000'},
}
Dissolve.hold = 0.7

function Dissolve:init(entry)
    Dissolve.super.init(self, entry)
    self.scenery = Scenery()
    self.automatic = true
    self.duration = 1.2
    self.subjectStates = {}
    for index, subject in ipairs(Dissolve.subjects) do
        local effect = {dissolve = 0, dissolveEdge = 0.08, dissolveSize = 6, dissolveColor = Dissolve.edgeColors[1].color}
        self.subjectStates[index] = {
            subject = subject,
            texture = assets.texture(subject.path, {filter = 'linear'}),
            target = 0,
            wait = index * 0.35,
            options = {scaleX = subject.scale, scaleY = subject.scale, pivotY = subject.pivotY or 1, layer = 2, effect = effect},
        }
    end
end

-- Every slider changes the effect of all the sprites, which keep one options table each, so nothing is created per frame.
function Dissolve:configure(key, value)
    for _, state in ipairs(self.subjectStates) do
        state.options.effect[key] = value
    end
end

function Dissolve:enter()
    local first = self.subjectStates[1].options.effect
    local colors = {}
    for index, edge in ipairs(Dissolve.edgeColors) do
        colors[index] = {id = edge.id, text = edge.text}
    end
    self:frame{
        hint = 'A click, a tap, E, Enter, Space or the south button dissolves the sprite nearest the cursor or brings it back.',
        cursor = true,
        controls = {
            ui.toggle{id = 'automatic', text = 'Dissolve by themselves', checked = self.automatic, onChange = function(event) self.automatic = event.checked end},
            ui.formField{label = 'Seconds to dissolve', ui.slider{id = 'duration', min = 0.3, max = 3, value = self.duration, showValue = true, onChange = function(event)
                self.duration = event.value
            end}},
            ui.formField{label = 'Edge width', ui.slider{id = 'edge', min = 0, max = 0.4, value = first.dissolveEdge, showValue = true, onChange = function(event)
                self:configure('dissolveEdge', event.value)
            end}},
            ui.formField{label = 'Noise cell size in pixels', ui.slider{id = 'size', min = 1, max = 24, value = first.dissolveSize, showValue = true, decimals = 0, onChange = function(event)
                self:configure('dissolveSize', event.value)
            end}},
            ui.formField{label = 'Edge color', ui.radioGroup{id = 'color', items = colors, selected = Dissolve.edgeColors[1].id, onChange = function(event)
                for _, edge in ipairs(Dissolve.edgeColors) do
                    if edge.id == event.value then
                        self:configure('dissolveColor', edge.color)
                    end
                end
            end}},
        },
    }
end

function Dissolve:nearest()
    local best, distance = nil, math.huge
    for _, state in ipairs(self.subjectStates) do
        local away = math.abs(state.subject.x - self.cursorX)
        if away < distance then
            best, distance = state, away
        end
    end
    return best
end

function Dissolve:update(dt)
    Dissolve.super.update(self, dt)
    self.scenery:update(dt)
    if self.stage and self:pressed() then
        local state = self:nearest()
        state.target = 1 - state.target
        state.wait = Dissolve.hold
    end

    for _, state in ipairs(self.subjectStates) do
        local effect = state.options.effect
        effect.dissolve = m.moveToward(effect.dissolve, state.target, dt / self.duration)
        if self.automatic and effect.dissolve == state.target then
            state.wait = state.wait - dt
            if state.wait <= 0 then
                state.target = 1 - state.target
                state.wait = Dissolve.hold
            end
        end
    end

    local states = self.subjectStates
    self:report('Dissolve %.2f, %.2f, %.2f, %.2f   Edge %.2f   Cell %.0f px', states[1].options.effect.dissolve, states[2].options.effect.dissolve, states[3].options.effect.dissolve, states[4].options.effect.dissolve, states[1].options.effect.dissolveEdge, states[1].options.effect.dissolveSize)
end

function Dissolve:draw(area)
    self.scenery:draw()
    for _, state in ipairs(self.subjectStates) do
        local subject = state.subject
        graphics2d.draw(state.texture, subject.x, Scenery.ground - (subject.y or 0), state.options)
        ParticleTest.label(subject.name, subject.x, Scenery.ground + 20)
    end
end

return Dissolve
