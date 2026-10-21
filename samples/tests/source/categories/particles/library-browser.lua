-- Effect library: every effect of `content/particles/library`, listed by category from its `catalog.json`. A one-shot effect fires again at the cursor on a click, a tap, E, Enter, Space or the south button and replays by itself, a looping effect follows the cursor, and the sliders scale the main values of every emitter of the effect live. Z and X, the page keys or the shoulder buttons pick the previous and next effect, and C or the north button the next category. Effects whose coins fly to a point of the world fly into the coin counter of the corner, and the grid that shows how distortion bends the image only appears behind effects that distort.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local code = require('harness.code')

local ParticleTest = require('categories.particles.particle-test')

local LibraryBrowser = haylen.class('LibraryBrowser', ParticleTest)

-- The code of the card, with the path of the effect that shows.
LibraryBrowser.code = [[
local system = particles2d.newSystem(assets.load('%s'), {seed = 7})
for _, emitter in ipairs(system:emitters()) do
    emitter:configure({rate = emitter.config.rate * 2})  -- Scales a value of every part.
end
system.scale = 1.5
system:restart()  -- A one-shot effect fires again.]]

LibraryBrowser.folder = 'particles/library/'
LibraryBrowser.styles = {casual = 'Casual', realistic = 'Realistic', pixel = 'Pixel art', neon = 'Neon'}
LibraryBrowser.kinds = {loop = 'loop', once = 'one shot', cycle = 'cycle'}
LibraryBrowser.views = {top = ', top view', side = ', side view', any = ''}
LibraryBrowser.replayDelay = 0.15
LibraryBrowser.listHeight = 320
-- The coin counter sits this far inside the top-right corner of the canvas.
LibraryBrowser.counterInset = {150, 70}
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
    self.coins = 0
    self.counter = {0, 0}
    self.coinLook = ParticleTest.look('coin-1', {})
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
    self.floorY = nil
    self.distorts = false
    self.collectors = {}
    for index, emitter in ipairs(self.emitters) do
        local config = emitter.config
        self.bases[index] = config
        if config.collision.type == 'floor' and not self.floorY then
            self.floorY = config.collision.y
        end
        self.distorts = self.distorts or config.distortion > 0
        for attractor, settings in ipairs(config.attractors) do
            if settings.space == 'world' then
                self.collectors[#self.collectors + 1] = {emitter = emitter, attractor = attractor, count = 0}
            end
        end
    end
    self:aimCollectors()
    if self.gui then
        self:showCode()
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

-- Points the coins of the effect at the coin counter, which follows the corner of the canvas.
function LibraryBrowser:aimCollectors()
    for _, collector in ipairs(self.collectors) do
        collector.emitter:setAttractor(collector.attractor, self.counter[1], self.counter[2])
    end
end

-- Counts the coins that the counter swallowed: the ones that vanished from an emitter whose coins fly to it.
function LibraryBrowser:collect()
    for _, collector in ipairs(self.collectors) do
        local count = collector.emitter.count
        if count < collector.count then
            self.coins = self.coins + collector.count - count
        end
        collector.count = count
    end
end

function LibraryBrowser:showCode()
    self.codeSource = string.format(LibraryBrowser.code, self.path)
    self:set('code', {text = code.markup(self.codeSource)})
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
        items[index] = {id = effect.id, text = effect.name, caption = LibraryBrowser.styles[effect.style] .. ', ' .. LibraryBrowser.kinds[effect.kind] .. LibraryBrowser.views[effect.view] .. (effect.composite and ', several emitters' or '')}
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
        ui.row{wrap = true, gap = 8,
            ui.button{id = 'previous', text = 'Previous', onClick = function() self:step(0, -1) end},
            ui.button{id = 'fire', text = 'Fire again', onClick = function() self:fire() end},
            ui.button{id = 'next', text = 'Next', onClick = function() self:step(0, 1) end},
        },
        ui.scroll{height = LibraryBrowser.listHeight, ui.list{id = 'effects', items = self:effectItems(self.categories[self.categoryIndex]), selected = self:current().effects[self.effectIndex].id, onSelect = function(event)
            for index, effect in ipairs(self.categories[self.categoryIndex].effects) do
                if effect.id == event.item then
                    self:choose(self.categoryIndex, index)
                end
            end
        end}},
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
        code = string.format(LibraryBrowser.code, self.path),
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
    if self.stage then
        local area = self.camera:visibleBounds()
        self.counter[1], self.counter[2] = area:right() - LibraryBrowser.counterInset[1], area.y + LibraryBrowser.counterInset[2]
        self:aimCollectors()
    end
    self.system:update(dt)
    self:collect()

    local category = self:current()
    self:report('%s: %s, effect %d of %d, with %d emitters and %d particles.', category.title, effect.name, self.effectIndex, #category.effects, #self.emitters, self.system.count)
end

-- A grid behind the effects shows how distortion effects bend the image, in dark lines on the light floor of the night.
function LibraryBrowser.drawGrid(area, color)
    for x = area.x - area.x % 80, area:right(), 80 do
        graphics2d.drawLine(x, area.y, x, area:bottom(), 2, color, {layer = -9})
    end
    for y = area.y - area.y % 80, area:bottom(), 80 do
        graphics2d.drawLine(area.x, y, area:right(), y, 2, color, {layer = -9})
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
    -- At night the floor is light stone that the darkness covers, so the lights of the effects show it around them, and the grid only shows behind effects that bend the image.
    if self.lit then
        ParticleTest.backdrop({0.5, 0.52, 0.6}, {0.62, 0.62, 0.68})
    else
        ParticleTest.backdrop({0.05, 0.06, 0.11}, {0.1, 0.11, 0.2})
    end
    if self.distorts then
        LibraryBrowser.drawGrid(bounds, self.lit and '#40101828' or '#24FFFFFF')
    end
    if self.floorY then
        ParticleTest.paveFloor('stone', self.system.y + self.floorY * self.system.scale, 140, '#FF6A6A80')
    end
    if #self.collectors > 0 then
        self:drawCounter()
    end
    self.system:draw()
end

-- Draws the coin counter of the corner as a chip of the interface with a coin and the number of coins it collected.
function LibraryBrowser:drawCounter()
    local x, y = self.counter[1], self.counter[2]
    graphics2d.drawShape({x - 54, y - 34, 190, 68}, {radius = 34, color = '#E02F3352', borderWidth = 4, borderColor = '#FF15172A', layer = 30})
    graphics2d.draw(self.coinLook.texture, x, y, {source = self.coinLook.frames[1], width = 56, height = 56, layer = 31})
    graphics2d.drawText(nil, tostring(self.coins), x + 44, y, {size = 36, anchor = {0, 0.5}, color = '#FFFFE08A', outlineWidth = 3, layer = 31})
end

return LibraryBrowser
