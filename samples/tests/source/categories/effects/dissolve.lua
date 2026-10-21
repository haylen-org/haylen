-- Dissolve: the `effect` of a draw hides the pixels of a sprite whose value noise falls under `dissolve` and colors a band along the border with `dissolveColor`, all on the GPU, so the sprites dissolve and come back without any texture of their own.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Scenery = require('categories.effects.scenery')

local Dissolve = haylen.class('Dissolve', ParticleTest)

Dissolve.code = [[
local options = {pivotY = 1, effect = {dissolve = 0, dissolveEdge = 0.08, dissolveSize = 10, dissolveColor = '#FFFF9A30'}}
options.effect.dissolve = m.moveToward(options.effect.dissolve, 1, dt / 1.2)  -- Runs from 0, whole, to 1, gone.
graphics2d.draw(texture, x, y, options)]]

Dissolve.subjects = {
    {name = 'Knight', path = 'sprites/images/hero.png', x = -600, scale = 1.4},
    {name = 'Rock', path = 'sprites/images/rock.png', x = -200, scale = 2.25},
    {name = 'Tree', path = 'sprites/images/tree.png', x = 200, scale = 1.7},
    {name = 'Chest', path = 'tiled/tiles/terrain.png', source = {662, 266, 128, 128}, x = 600, scale = 1.6, pivotY = 0.92},
}
Dissolve.edgeColors = {
    {id = 'embers', text = 'Embers', color = '#FFFF9A30'},
    {id = 'magic', text = 'Magic', color = '#FF70E4FF'},
    {id = 'toxic', text = 'Toxic', color = '#FFA4FF40'},
    {id = 'none', text = 'No edge color', color = '#00000000'},
}
Dissolve.hold = 0.7
-- By themselves the sprites take turns, one dissolving and coming back at a time, after a short wait as the test opens.
Dissolve.firstWait = 0.6

function Dissolve:init(entry)
    Dissolve.super.init(self, entry)
    self.scenery = Scenery()
    self.automatic = true
    self.duration = 1.2
    self.turn, self.wait = 1, Dissolve.firstWait
    self.labelStyle = {size = 30, anchor = {0.5, 0}, outlineWidth = 3, color = {1, 1, 1, 1}, outlineColor = {0, 0, 0, 1}, layer = 20}
    self.subjectStates = {}
    for index, subject in ipairs(Dissolve.subjects) do
        local effect = {dissolve = 0, dissolveEdge = 0.08, dissolveSize = 10, dissolveColor = Dissolve.edgeColors[1].color}
        self.subjectStates[index] = {
            subject = subject,
            texture = assets.texture(subject.path, {filter = 'linear'}),
            target = 0,
            options = {source = subject.source, scaleX = subject.scale, scaleY = subject.scale, pivotY = subject.pivotY or 1, layer = 2, effect = effect},
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
        code = Dissolve.code,
        cursor = true,
        controls = {
            ui.toggle{id = 'automatic', text = 'Dissolve by themselves', checked = self.automatic, onChange = function(event) self.automatic = event.checked end},
            ui.formField{label = 'Seconds to dissolve', ui.slider{id = 'duration', min = 0.3, max = 3, value = self.duration, showValue = true, onChange = function(event)
                self.duration = event.value
            end}},
            ui.formField{label = 'Edge width', ui.slider{id = 'edge', min = 0, max = 0.4, value = first.dissolveEdge, showValue = true, onChange = function(event)
                self:configure('dissolveEdge', event.value)
            end}},
            ui.formField{label = 'Noise cell size in pixels', ui.slider{id = 'size', min = 1, max = 48, value = first.dissolveSize, showValue = true, decimals = 0, onChange = function(event)
                self:configure('dissolveSize', event.value)
            end}},
            ui.formField{label = 'Edge color', ui.combo{id = 'color', items = colors, selected = Dissolve.edgeColors[1].id, onChange = function(event)
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
    end

    for _, state in ipairs(self.subjectStates) do
        local effect = state.options.effect
        effect.dissolve = m.moveToward(effect.dissolve, state.target, dt / self.duration)
    end
    if self.automatic then
        self:takeTurns(dt)
    end

    local states = self.subjectStates
    self:report('Dissolved %.2f, %.2f, %.2f and %.2f, edge %.2f, cells of %.0f px', states[1].options.effect.dissolve, states[2].options.effect.dissolve, states[3].options.effect.dissolve, states[4].options.effect.dissolve, states[1].options.effect.dissolveEdge, states[1].options.effect.dissolveSize)
end

-- The sprite whose turn it is dissolves, waits gone, comes back and waits whole, and then the next one takes its turn.
function Dissolve:takeTurns(dt)
    local state = self.subjectStates[self.turn]
    if state.options.effect.dissolve ~= state.target then
        return
    end
    self.wait = self.wait - dt
    if self.wait > 0 then
        return
    end
    self.wait = Dissolve.hold
    if state.target == 0 then
        state.target = 1
        return
    end
    state.target = 0
    self.turn = self.turn % #self.subjectStates + 1
end

-- A name fades with its sprite, so no name points at a sprite that is gone.
function Dissolve:draw(area)
    self.scenery:draw()
    local style = self.labelStyle
    for _, state in ipairs(self.subjectStates) do
        local subject = state.subject
        graphics2d.draw(state.texture, subject.x, Scenery.ground, state.options)
        local shown = 1 - state.options.effect.dissolve
        style.color[4], style.outlineColor[4] = shown, shown
        graphics2d.drawText(nil, subject.name, subject.x, Scenery.ground + 20, style)
    end
end

return Dissolve
