-- Effect library: every effect of `content/particles/library`, listed by category from its `catalog.json`. A one-shot effect fires again at the cursor on a click, a tap, E, Enter, Space or the south button and replays by itself, a looping effect follows the cursor, and the sliders scale the main values of every emitter of the effect live. Z and X, the page keys or the shoulder buttons pick the previous and next effect, and C or the north button the next category.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local LibraryBrowser = haylen.class('LibraryBrowser', ParticleTest)

LibraryBrowser.folder = 'particles/library/'
LibraryBrowser.styles = {realistic = 'Realistic', cartoon = 'Cartoon', pixel = 'Pixel art', neon = 'Neon', magic = 'Magic', scifi = 'Science fiction'}
LibraryBrowser.kinds = {loop = 'loop', once = 'one shot', cycle = 'cycle'}
LibraryBrowser.counter = {760, -420}
LibraryBrowser.replayDelay = 0.6
LibraryBrowser.sliders = {
    {'scale', 'Scale', 0.25, 3},
    {'rate', 'Rate', 0, 3},
    {'speed', 'Speed', 0, 3},
    {'lifetime', 'Lifetime', 0.25, 3},
    {'spread', 'Spread', 0, 2},
    {'gravity', 'Gravity', -1, 3},
    {'damping', 'Damping', 0, 3},
}
LibraryBrowser.actions = {actions = {
    ParticleTest.actions.actions[1],
    ParticleTest.actions.actions[2],
    ParticleTest.actions.actions[3],
    {name = 'previous', type = 'button', bindings = {'key:z', 'key:pageUp', 'button:leftShoulder'}},
    {name = 'next', type = 'button', bindings = {'key:x', 'key:pageDown', 'button:rightShoulder'}},
    {name = 'category', type = 'button', bindings = {'key:c', 'button:north'}},
}}

function LibraryBrowser:init(entry)
    LibraryBrowser.super.init(self, entry)
    self.categories = assets.json(LibraryBrowser.folder .. 'catalog.json').categories
    self.categoryIndex, self.effectIndex = 1, 1
    self.lit = false
    self.values = {}
    self:resetValues()
    self:choose(1, 1)
end

function LibraryBrowser:resetValues()
    for _, spec in ipairs(LibraryBrowser.sliders) do
        self.values[spec[1]] = 1
    end
end

function LibraryBrowser:current()
    local category = self.categories[self.categoryIndex]
    return category, category.effects[self.effectIndex]
end

-- Loads an effect as a system, which serves composite effects and effects of one emitter alike, and keeps the configuration of each emitter that the sliders scale.
function LibraryBrowser:choose(categoryIndex, effectIndex)
    self.categoryIndex, self.effectIndex = categoryIndex, effectIndex
    local category, effect = self:current()
    self.path = LibraryBrowser.folder .. category.id .. '/' .. effect.id .. '.particles'
    self.system = particles2d.newSystem(assets.load(self.path), {seed = 7})
    self.emitters = self.system:emitters()
    self.bases = {}
    self.floor = nil
    for index, emitter in ipairs(self.emitters) do
        local config = emitter.config
        self.bases[index] = config
        if config.collision.type == 'floor' and not self.floor then
            self.floor = config.collision.y
        end
        for attractor, settings in ipairs(config.attractors) do
            if settings.space == 'world' then
                emitter:setAttractor(attractor, LibraryBrowser.counter[1], LibraryBrowser.counter[2])
            end
        end
    end
    self.waiting = 0
    self:place()
    self:apply()
    self.system:restart()
end

-- Weather covers the view from above it, and every other effect sits at the cursor.
function LibraryBrowser:place()
    local category = self:current()
    if category.id == 'weather' then
        self.system.position = {0, -600}
    else
        self.system.position = {self.cursorX, self.cursorY}
    end
end

function LibraryBrowser.scaled(range, factor)
    return {range[1] * factor, range[2] * factor}
end

-- Every slider multiplies the value the effect file gives each emitter, and the scale grows the whole effect.
function LibraryBrowser:apply()
    local values = self.values
    self.system.scale = values.scale
    for index, emitter in ipairs(self.emitters) do
        local base = self.bases[index]
        emitter:configure({
            rate = base.rate * values.rate,
            speed = LibraryBrowser.scaled(base.speed, values.speed),
            lifetime = LibraryBrowser.scaled(base.lifetime, values.lifetime),
            spread = base.spread * values.spread,
            gravity = {base.gravity.x * values.gravity, base.gravity.y * values.gravity},
            damping = base.damping * values.damping,
        })
    end
end

function LibraryBrowser:effectItems(category)
    local items = {}
    for index, effect in ipairs(category.effects) do
        items[index] = {id = effect.id, text = effect.name, caption = LibraryBrowser.styles[effect.style] .. ', ' .. LibraryBrowser.kinds[effect.kind] .. (effect.composite and ', several emitters' or '')}
    end
    return items
end

function LibraryBrowser:showSelection()
    local category, effect = self:current()
    self:set('category', {selected = category.id})
    self:set('effects', {items = self:effectItems(category), selected = effect.id})
end

function LibraryBrowser:step(categoryStep, effectStep)
    local categoryIndex = (self.categoryIndex - 1 + categoryStep) % #self.categories + 1
    local count = #self.categories[categoryIndex].effects
    local effectIndex = categoryStep ~= 0 and 1 or (self.effectIndex - 1 + effectStep) % count + 1
    self:choose(categoryIndex, effectIndex)
    self:showSelection()
end

function LibraryBrowser:enter()
    local categories = {}
    for index, category in ipairs(self.categories) do
        categories[index] = {id = category.id, text = category.title}
    end
    local controls = {
        ui.formField{label = 'Category', ui.combo{id = 'category', items = categories, selected = self.categories[self.categoryIndex].id, onChange = function(event)
            for index, category in ipairs(self.categories) do
                if category.id == event.value then
                    self:choose(index, 1)
                    self:showSelection()
                end
            end
        end}},
        ui.list{id = 'effects', items = self:effectItems(self.categories[self.categoryIndex]), selected = self:current().effects[self.effectIndex].id, onSelect = function(event)
            for index, effect in ipairs(self.categories[self.categoryIndex].effects) do
                if effect.id == event.item then
                    self:choose(self.categoryIndex, index)
                end
            end
        end},
        ui.row{gap = 8,
            ui.button{id = 'previous', text = 'Previous', onClick = function() self:step(0, -1) end},
            ui.button{id = 'fire', text = 'Fire again', onClick = function() self:fire() end},
            ui.button{id = 'next', text = 'Next', onClick = function() self:step(0, 1) end},
        },
        ui.toggle{id = 'lit', text = 'Night with lights', checked = self.lit, onChange = function(event) self.lit = event.checked end},
    }
    for _, spec in ipairs(LibraryBrowser.sliders) do
        local key = spec[1]
        controls[#controls + 1] = ui.formField{label = spec[2], ui.slider{id = key, min = spec[3], max = spec[4], value = self.values[key], showValue = true, decimals = 2, onChange = function(event)
            self.values[key] = event.value
            self:apply()
        end}}
    end
    controls[#controls + 1] = ui.button{id = 'reset', text = 'Reset values', onClick = function()
        self:resetValues()
        for _, spec in ipairs(LibraryBrowser.sliders) do
            self:set(spec[1], {value = 1})
        end
        self:apply()
    end}

    self:frame{
        hint = 'A click, a tap, E, Enter, Space or the south button fires the effect at the cursor. Z and X, the page keys or the shoulder buttons pick the previous and next effect, and C or the north button the next category.',
        cursor = true,
        controls = controls,
    }
    self:loadActions(LibraryBrowser.actions)
end

function LibraryBrowser:fire()
    self:place()
    self.system:restart()
    self.waiting = 0
end

function LibraryBrowser:update(dt)
    LibraryBrowser.super.update(self, dt)
    if input.pressed('previous') then
        self:step(0, -1)
    elseif input.pressed('next') then
        self:step(0, 1)
    elseif input.pressed('category') then
        self:step(1, 0)
    end

    local _, effect = self:current()
    if self:pressed() then
        self:fire()
    elseif effect.kind ~= 'once' then
        self:place()
    elseif not self.system.alive then
        -- One-shot effects replay on their own after a short pause, so every effect keeps showing.
        self.waiting = self.waiting + dt
        if self.waiting >= LibraryBrowser.replayDelay then
            self:fire()
        end
    end
    self.system:update(dt)

    local category = self:current()
    self:report('%s, "%s"   Effect %d of %d   Emitters %d   Particles %d', category.title, self.path, self.effectIndex, #category.effects, #self.emitters, self.system.count)
end

-- A grid behind the effects shows how distortion effects bend the image.
function LibraryBrowser.drawGrid(area)
    for x = area.x - area.x % 80, area:right(), 80 do
        graphics2d.drawLine(x, area.y, x, area:bottom(), 2, '#30FFFFFF', {layer = -9})
    end
    for y = area.y - area.y % 80, area:bottom(), 80 do
        graphics2d.drawLine(area.x, y, area:right(), y, 2, '#30FFFFFF', {layer = -9})
    end
end

function LibraryBrowser:render()
    if not self.area then
        return
    end
    graphics2d.beginWorld(self.camera, {ambientLight = self.lit and '#FF303050' or nil, postProcess = {distortion = 32}})
    graphics2d.drawRect(graphics2d.canvasBounds(), ParticleTest.stageColor, {layer = -1000})
    self:draw(self.area)
end

function LibraryBrowser:draw(area)
    local bounds = graphics2d.canvasBounds()
    ParticleTest.backdrop({0.05, 0.06, 0.11}, {0.1, 0.11, 0.2})
    LibraryBrowser.drawGrid(bounds)
    if self.floor then
        local y = self.system.y + self.floor * self.system.scale
        graphics2d.drawRect({bounds.x, y, bounds.width, bounds:bottom() - y}, '#FF2A2F45', {layer = -8})
    end
    graphics2d.drawCircle(LibraryBrowser.counter[1], LibraryBrowser.counter[2], 26, '#FFFFD84A', {layer = 1})
    ParticleTest.label('Counter', LibraryBrowser.counter[1], LibraryBrowser.counter[2] + 36)
    self.system:draw()
end

return LibraryBrowser
