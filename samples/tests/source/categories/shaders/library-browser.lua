-- Shader library: every material shader of `content/shaders/library`, listed by category from its `catalog.json`, drawn on what it is made for over the island at night: the knight for sprite effects, flying bullets for neon and plasma, a trail for strokes, a sweeping beam for lasers and lightning, a shield over the knight, a rectangle for fire and lava, and a picture for styles and deformations. The sliders set the numbers of the selected shader live, and the card shows its code.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ShaderTest = require('categories.shaders.shader-test')
local code = require('harness.code')

local LibraryBrowser = haylen.class('LibraryBrowser', ShaderTest)

LibraryBrowser.folder = 'shaders/library/'
-- The textures each target draws, as the code of the card names them.
LibraryBrowser.targetNames = {sprite = 'knight', disc = 'graphics.whiteTexture()', bullet = 'bullet', stroke = 'trail', beam = 'beam', quad = 'graphics.whiteTexture()', picture = 'picture'}
-- The bullet is a capsule of soft light with a wide clear margin, which the glow of neon and plasma shaders fills without reaching the edge of the quad.
LibraryBrowser.bulletSize = {320, 160}
LibraryBrowser.bulletCore = {75, 13}
LibraryBrowser.targets = {sprite = 'on a sprite', bullet = 'on bullets', stroke = 'on a trail', beam = 'on a beam', disc = 'on a shield', quad = 'on a rectangle', picture = 'on a picture'}
-- The trail keeps the points of its last second, however many frames that is.
LibraryBrowser.trailSeconds = 1
-- The hero keeps its size on screen inside the margin its image gains.
LibraryBrowser.heroMargin = 24
LibraryBrowser.heroSize = 460 * (192 + 2 * 24) / 192

function LibraryBrowser:init(entry)
    LibraryBrowser.super.init(self, entry)
    self.catalog = assets.json(LibraryBrowser.folder .. 'catalog.json').categories
    self.hero = graphics.newTexture(LibraryBrowser.makeHero(), {filter = 'linear'})
    self.picture = assets.texture('interface/images/landscape.png', {filter = 'linear'})
    self.bullet = graphics.newTexture(LibraryBrowser.makeBullet(), {filter = 'linear'})
    self.categoryIndex, self.shaderIndex = 1, 1
    self.trail = {}
end

-- The hero with a transparent margin around it, which the shadows, glows and outlines of sprite shaders fill.
function LibraryBrowser.makeHero()
    local hero = graphics.decodeImage(assets.bytes('sprites/images/hero.png'))
    local margin = LibraryBrowser.heroMargin
    local image = graphics.newImage(hero.width + margin * 2, hero.height + margin * 2)
    image:blit(hero, margin, margin)
    image:bleed()
    return image
end

-- A bright capsule whose edge fades over a few pixels, so the glow that the shaders gather around it stays smooth.
function LibraryBrowser.makeBullet()
    local width, height = LibraryBrowser.bulletSize[1], LibraryBrowser.bulletSize[2]
    local length, radius = LibraryBrowser.bulletCore[1], LibraryBrowser.bulletCore[2]
    local image = graphics.newImage(width, height)
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local dx = math.max(math.abs(x - width / 2 + 0.5) - length, 0)
            local dy = math.abs(y - height / 2 + 0.5)
            local distance = math.sqrt(dx * dx + dy * dy)
            local alpha = 1 - math.max(0, math.min(1, (distance - radius + 3) / 6))
            if alpha > 0 then
                image:setPixel(x, y, {1, 1, 1, alpha * alpha * (3 - 2 * alpha)})
            end
        end
    end
    return image
end

function LibraryBrowser:current()
    local category = self.catalog[self.categoryIndex]
    return category, category.shaders[self.shaderIndex]
end

-- The texture each target draws, whose texel the shaders that read the pixels around them need.
function LibraryBrowser:texture(target)
    if target == 'sprite' or target == 'disc' then
        return self.hero
    elseif target == 'bullet' then
        return self.bullet
    elseif target == 'picture' then
        return self.picture
    end
    return graphics.whiteTexture()
end

function LibraryBrowser:choose(categoryIndex, shaderIndex)
    self.categoryIndex, self.shaderIndex = categoryIndex, shaderIndex
    local _, entry = self:current()
    local uniforms = {}
    for _, spec in ipairs(entry.floats) do
        uniforms[spec.name] = spec.value
    end
    for name, value in pairs(entry.colors) do
        uniforms[name] = value
    end
    for name, value in pairs(entry.vectors) do
        uniforms[name] = value
    end
    if entry.texel then
        local texture = self:texture(entry.target)
        uniforms.texel = {1 / texture.width, 1 / texture.height}
    end
    self.material = graphics2d.newMaterial(assets.shader(LibraryBrowser.folder .. entry.id .. '.shader'), uniforms)
    self.shader = entry
    self.codeSource = LibraryBrowser.source(entry)
    if self.gui then
        self.gui:set('code', {text = code.markup(self.codeSource)})
    end
end

-- Writes the code that draws a shader of the library with the values of its catalog entry.
function LibraryBrowser.source(entry)
    local lines = {string.format("local material = graphics2d.newMaterial(assets.shader('%s%s.shader'))", LibraryBrowser.folder, entry.id)}
    for _, spec in ipairs(entry.floats) do
        if #lines < 4 then
            lines[#lines + 1] = string.format("material:set('%s', %g)", spec.name, spec.value)
        end
    end
    for name, value in pairs(entry.colors) do
        if #lines < 5 then
            lines[#lines + 1] = string.format("material:set('%s', '%s')", name, value)
        end
    end
    local blend = entry.blend == 'additive' and ", blend = 'additive'" or ''
    lines[#lines + 1] = string.format('graphics2d.draw(%s, x, y, {material = material%s})', LibraryBrowser.targetNames[entry.target], blend)
    return table.concat(lines, '\n')
end

function LibraryBrowser:shaderItems(category)
    local items = {}
    for index, entry in ipairs(category.shaders) do
        items[index] = {id = entry.id, text = entry.name, caption = LibraryBrowser.targets[entry.target]}
    end
    return items
end

-- The sliders of the selected shader replace those of the one before.
function LibraryBrowser:sliders()
    local nodes = {}
    for _, spec in ipairs(self.shader.floats) do
        nodes[#nodes + 1] = ui.formField{label = spec.label, ui.slider{id = 'uniform-' .. spec.name, min = spec.min, max = spec.max, value = spec.value, showValue = true, decimals = 2, onChange = function(event)
            self.material:set(spec.name, event.value)
        end}}
    end
    if #nodes == 0 then
        nodes[1] = ui.label{text = 'This shader has no numbers to set.', font = 'caption', color = 'textMuted'}
    end
    return nodes
end

function LibraryBrowser:showSelection()
    local category, entry = self:current()
    self:set('libraryCategory', {selected = category.id})
    self:set('libraryShaders', {items = self:shaderItems(category), selected = entry.id})
    self:set('libraryDescription', {text = entry.description})
    self.gui:replaceChildren('libraryUniforms', self:sliders())
end

function LibraryBrowser:enter()
    self:choose(1, 1)
    local categories = {}
    for index, category in ipairs(self.catalog) do
        categories[index] = {id = category.id, text = category.title}
    end
    self:frame{
        code = self.codeSource,
        hint = 'Pick a category and a shader, and move its sliders. The shaders are ordinary material shaders that any app copies with their "common.glsl".',
        controls = {
            ui.formField{label = 'Category', ui.combo{id = 'libraryCategory', items = categories, selected = self.catalog[1].id, onChange = function(event)
                for index, category in ipairs(self.catalog) do
                    if category.id == event.value then
                        self:choose(index, 1)
                        self:showSelection()
                    end
                end
            end}},
            ui.scroll{height = 330, ui.list{id = 'libraryShaders', items = self:shaderItems(self.catalog[1]), selected = self.shader.id, onSelect = function(event)
                for index, entry in ipairs(self.catalog[self.categoryIndex].shaders) do
                    if entry.id == event.item then
                        self:choose(self.categoryIndex, index)
                        self:showSelection()
                    end
                end
            end}},
            ui.column{id = 'libraryUniforms', gap = 8, children = self:sliders()},
            ui.label{id = 'libraryDescription', text = self.shader.description, font = 'caption', color = 'textMuted'},
        },
    }
end

function LibraryBrowser:update(dt)
    LibraryBrowser.super.update(self, dt)
    local time = haylen.elapsed()
    if self.shader.time then
        self.material:set('time', time)
    end
    -- The head of the trail draws a figure eight, and the trail keeps the points of its last second.
    table.insert(self.trail, 1, {670 + math.sin(time * 1.3) * 420, 380 + math.sin(time * 2.6) * 200, time})
    while #self.trail > 2 and time - self.trail[#self.trail][3] > LibraryBrowser.trailSeconds do
        self.trail[#self.trail] = nil
    end
    self:status(string.format('The shader "%s" draws from "%s%s.shader".', self.shader.name, LibraryBrowser.folder, self.shader.id))
end

function LibraryBrowser:draw(area)
    local order = {material = self.material, blend = self.shader.blend == 'additive' and 'additive' or 'alpha'}
    local target = self.shader.target
    local width, height = ShaderTest.layout[1], ShaderTest.layout[2]
    if target ~= 'picture' then
        local bounds = graphics2d.canvasBounds()
        self:drawCard({rect = {bounds.x, bounds.y, bounds.width, bounds.height}}, self.shader.name, target ~= 'sprite' and target ~= 'disc')
    end
    if target == 'sprite' then
        graphics2d.draw(self.hero, width / 2, height / 2, {width = LibraryBrowser.heroSize, height = LibraryBrowser.heroSize, material = order.material, blend = order.blend})
    elseif target == 'disc' then
        graphics2d.draw(self.hero, width / 2, height / 2, {width = 450, height = 450})
        graphics2d.draw(graphics.whiteTexture(), width / 2, height / 2, {width = 470, height = 470, material = order.material, blend = order.blend, layer = 1})
    elseif target == 'bullet' then
        local time = haylen.elapsed()
        for index = 1, 3 do
            local x = (time * 420 + index * 460) % (width + 320) - 160
            graphics2d.draw(self.bullet, x, 150 + index * 140, {width = LibraryBrowser.bulletSize[1], height = LibraryBrowser.bulletSize[2], material = order.material, blend = order.blend, layer = 5})
        end
    elseif target == 'stroke' then
        local points = {}
        for index = #self.trail, 1, -1 do
            points[#points + 1] = self.trail[index][1]
            points[#points + 1] = self.trail[index][2]
        end
        if #points >= 4 then
            graphics2d.drawStroke(points, {width = 64, join = 'round', material = order.material, blend = order.blend})
        end
    elseif target == 'beam' then
        -- The beam runs from an emitter on the left to a point that sweeps slowly up and down.
        local angle = math.sin(haylen.elapsed() * 0.7) * 0.32
        graphics2d.drawStroke({170, 380, 170 + math.cos(angle) * 1000, 380 + math.sin(angle) * 1000}, {width = 150, material = order.material, blend = order.blend})
    elseif target == 'quad' then
        graphics2d.draw(graphics.whiteTexture(), width / 2, height / 2, {width = 520, height = 560, material = order.material, blend = order.blend})
    else
        graphics2d.draw(self.picture, width / 2, height / 2, {width = 1080, height = 608, material = order.material, blend = order.blend})
    end
end

return LibraryBrowser
