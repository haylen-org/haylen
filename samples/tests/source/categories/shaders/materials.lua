-- Sprite materials: seven custom fragment shaders, each shading one sprite through `graphics2d.newMaterial` and the `material` key of the draw.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local ShaderTest = require('categories.shaders.shader-test')

local Materials = haylen.class('Materials', ShaderTest)

-- The colors of the hero image, and the palettes the swap turns them into.
Materials.source = {'#FF1B1E2B', '#FF3AA86B', '#FF2A7A4E', '#FF8FE0A0', '#FFFFFFFF', '#FFE0543A'}
Materials.palettes = {
    {'#FF1B1E2B', '#FF3A6AE0', '#FF2A4AA0', '#FF90B8FF', '#FFFFFFFF', '#FFFFD040'},
    {'#FF2B1B1E', '#FFE0503A', '#FFA0302A', '#FFFFA090', '#FFFFFFFF', '#FF40E0FF'},
    {'#FF101010', '#FFB0B0B8', '#FF707078', '#FFF0F0F8', '#FF101010', '#FF505058'},
}
Materials.shown = {
    {'Dissolve', 'planet', 'dissolve'},
    {'Outline', 'hero', 'outline'},
    {'Hit flash', 'hero', 'flash'},
    {'Wave', 'planet', 'wave'},
    {'Palette swap', 'hero', 'palette'},
    {'Hologram', 'hero', 'hologram'},
    {'Pixelation', 'planet', 'pixelate'},
}

-- Flattens a list of colors into the numbers of a `vec4` array uniform.
function Materials.colors(list)
    local numbers = {}
    for _, text in ipairs(list) do
        local color = m.color(text)
        table.move({color.r, color.g, color.b, color.a}, 1, 4, #numbers + 1, numbers)
    end
    return numbers
end

function Materials:init(entry)
    Materials.super.init(self, entry)
    self.images = {hero = ShaderTest.hero(), planet = ShaderTest.planet()}
    self.palette = 1
    self.flash = 0
    local hero = self.images.hero
    self.materials = {
        dissolve = ShaderTest.material('dissolve', {edge_color = '#FFFF9030', edge_width = 0.08, noise_scale = 1, noise_texture = ShaderTest.noise()}),
        outline = ShaderTest.material('outline', {outline_color = '#FFFFE040', texel = {1 / hero.width, 1 / hero.height}, thickness = 1}),
        flash = ShaderTest.material('flash', {flash_color = '#FFFFFFFF'}),
        wave = ShaderTest.material('wave', {amplitude = 0.02, frequency = 18, speed = 3}),
        palette = ShaderTest.material('palette', {source_colors = Materials.colors(Materials.source), target_colors = Materials.colors(Materials.palettes[1])}),
        hologram = ShaderTest.material('hologram', {tint = '#FF40E0FF', lines = 1.2}),
        pixelate = ShaderTest.material('pixelate'),
    }
    self.cells = ShaderTest.cells(#Materials.shown, 4)
end

function Materials:enter()
    self:frame{
        hint = 'A click or a tap on the play area, E, Enter or the south button flashes the hero. The palette button swaps its colors.',
        presses = true,
        controls = {
            ui.button{id = 'flash', text = 'Flash the hero', onClick = function()
                self.flash = 1
            end},
            ui.button{id = 'palette', text = 'Next palette', onClick = function()
                self.palette = self.palette % #Materials.palettes + 1
                self.materials.palette:set('target_colors', Materials.colors(Materials.palettes[self.palette]))
            end},
        },
    }
end

function Materials:update(dt)
    Materials.super.update(self, dt)
    if self:pressed() then
        self.flash = 1
    end
    self.flash = math.max(0, self.flash - dt * 3)

    local time = haylen.elapsed()
    local materials = self.materials
    materials.dissolve:set('amount', 0.5 + 0.5 * math.sin(time * 1.2))
    materials.outline:set('thickness', 1 + (math.sin(time * 4) + 1) * 0.5)
    materials.flash:set('amount', self.flash)
    materials.wave:set('time', time)
    materials.hologram:set('time', time)
    local blocks = 8 + (math.sin(time) + 1) * 28
    materials.pixelate:set('blocks', {blocks, blocks})
    self:status(string.format('Dissolve amount %.2f   Pixelate blocks %.0f   Palette %d', materials.dissolve:get('amount'), blocks, self.palette))
end

function Materials:draw(area)
    for index, entry in ipairs(Materials.shown) do
        local cell = self.cells[index]
        graphics2d.draw(self.images[entry[2]], cell.x, cell.y, {width = cell.size, height = cell.size, material = self.materials[entry[3]]})
        ShaderTest.caption(entry[1], cell.x, cell.y + cell.size / 2 + 8)
    end
end

return Materials
