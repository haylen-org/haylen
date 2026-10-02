-- Sprite materials: seven custom fragment shaders, each shading one sprite through `graphics2d.newMaterial` and the `material` key of the draw.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local art = require('art')
local sample = require('sample')

local Materials = haylen.class('Materials', sample.Test)

Materials.hints = 'Click, tap, E or the X button flashes the hero. The palette button swaps its colors.'

-- The colors of the hero image, and the palettes the swap turns them into.
Materials.source = {'#FF1B1E2B', '#FF3AA86B', '#FF2A7A4E', '#FF8FE0A0', '#FFFFFFFF', '#FFE0543A'}
Materials.palettes = {
    {'#FF1B1E2B', '#FF3A6AE0', '#FF2A4AA0', '#FF90B8FF', '#FFFFFFFF', '#FFFFD040'},
    {'#FF2B1B1E', '#FFE0503A', '#FFA0302A', '#FFFFA090', '#FFFFFFFF', '#FF40E0FF'},
    {'#FF101010', '#FFB0B0B8', '#FF707078', '#FFF0F0F8', '#FF101010', '#FF505058'},
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
    self.hero = art.hero()
    self.planet = art.planet()
    self.palette = 1
    self.flash = 0
    self.materials = {
        dissolve = art.material('dissolve', {edge_color = '#FFFF9030', edge_width = 0.08, noise_scale = 1, noise_texture = art.noise()}),
        outline = art.material('outline', {outline_color = '#FFFFE040', texel = {1 / self.hero.width, 1 / self.hero.height}, thickness = 1}),
        flash = art.material('flash', {flash_color = '#FFFFFFFF'}),
        wave = art.material('wave', {amplitude = 0.02, frequency = 18, speed = 3}),
        palette = art.material('palette', {source_colors = Materials.colors(Materials.source), target_colors = Materials.colors(Materials.palettes[1])}),
        hologram = art.material('hologram', {tint = '#FF40E0FF', lines = 1.2}),
        pixelate = art.material('pixelate'),
    }
end

function Materials:controls()
    return {
        ui.button{text = 'Flash the hero', onClick = function()
            self.flash = 1
        end},
        ui.button{text = 'Next palette', onClick = function()
            self.palette = self.palette % #Materials.palettes + 1
            self.materials.palette:set('target_colors', Materials.colors(Materials.palettes[self.palette]))
        end},
    }
end

function Materials:update(dt)
    if sample.pressed() then
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
    self:setStatus(string.format('Dissolve amount %.2f, pixelate blocks %.0f, palette %d', materials.dissolve:get('amount'), blocks, self.palette))
end

function Materials:render()
    graphics2d.beginScreen()
    local cells = art.cells(7, 4)
    local shown = {
        {'Dissolve', self.planet, 'dissolve'},
        {'Outline', self.hero, 'outline'},
        {'Hit flash', self.hero, 'flash'},
        {'Wave', self.planet, 'wave'},
        {'Palette swap', self.hero, 'palette'},
        {'Hologram', self.hero, 'hologram'},
        {'Pixelation', self.planet, 'pixelate'},
    }
    for index, entry in ipairs(shown) do
        local cell = cells[index]
        graphics2d.draw(entry[2], cell.x, cell.y, {width = cell.size, height = cell.size, material = self.materials[entry[3]]})
        art.caption(entry[1], cell.x, cell.y + cell.size / 2 + 8)
    end
end

return Materials
