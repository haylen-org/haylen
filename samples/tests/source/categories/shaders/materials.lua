-- Sprite materials: eight custom fragment shaders, each shading a painted sprite through `graphics2d.newMaterial` and the `material` key of the draw: a crate that burns away, a hero picked out by an outline, a bat that blinks white as it takes a hit, a flag in the wind, a hero in the colors of another team, a knight projected as a hologram, a planet that breaks into big pixels and a dragon that shines in a rainbow.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local ShaderTest = require('categories.shaders.shader-test')

local Materials = haylen.class('Materials', ShaderTest)

Materials.code = [[
local outline = graphics2d.newMaterial(assets.shader('shaders/outline.shader'))
outline:set('outline_color', '#FFFFC93C')
outline:set('texel', {1 / sheet.width, 1 / sheet.height})
flash:set('amount', 1)  -- A hit sets it to 1, and it fades back to 0.
graphics2d.draw(sheet, x, y, {source = idle, material = outline})
graphics2d.draw(knight, x, y, {material = hologram, blend = 'additive'})]]

-- The colors of the parts of the hero, its headband, vest, trousers and boots, and the teams the swap dresses it in.
Materials.source = {'#FFF0D030', '#FF8030B0', '#FF2050B0', '#FFC03020'}
Materials.palettes = {
    {'#FF4CD45A', '#FF2FB3C4', '#FF2F3352', '#FF6B4A2A'},
    {'#FFF0473E', '#FF2F3352', '#FF5D6596', '#FF1E1B33'},
    {'#FFFFFFFF', '#FFF0473E', '#FFFFC93C', '#FF2F86F6'},
}
-- Each cell with its title, its picture, its material, a night sky for the materials that glow, and the blend of its draw.
Materials.shown = {
    {'Dissolve', 'crate', 'dissolve'},
    {'Outline', 'walker', 'outline'},
    {'Hit flash', 'bat', 'flash'},
    {'Wave', 'flag', 'wave'},
    {'Palette swap', 'walker', 'palette'},
    {'Hologram', 'knight', 'hologram', true, 'additive'},
    {'Pixelation', 'planet', 'pixelate', true},
    {'Rainbow', 'dragon', 'rainbow', true},
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
    self.palette = 1
    self.flash = 0
    self.materials = {
        dissolve = ShaderTest.material('dissolve', {edge_color = '#FFFFB347', edge_width = 0.12, noise_scale = 1, noise_texture = ShaderTest.noise()}),
        outline = ShaderTest.material('outline', {outline_color = '#FFFFC93C', texel = ShaderTest.texel('walker'), thickness = 6}),
        flash = ShaderTest.material('flash', {flash_color = '#FFFFFFFF'}),
        wave = ShaderTest.material('wave', {amplitude = 0.025, frequency = 12, speed = 4}),
        palette = ShaderTest.material('palette', {source_colors = Materials.colors(Materials.source), target_colors = Materials.colors(Materials.palettes[1])}),
        hologram = ShaderTest.material('hologram', {tint = '#FF40E0FF', lines = 1.2, texel = ShaderTest.texel('knight')}),
        pixelate = ShaderTest.material('pixelate'),
        rainbow = ShaderTest.material('rainbow', {spread = 0.004}),
    }
    self.cells = ShaderTest.cells(#Materials.shown, 4)
end

function Materials:enter()
    self:frame{
        code = Materials.code,
        hint = 'A click or a tap on the play area, E, Enter or the south button hits the bat. The palette button dresses the hero in the colors of another team.',
        presses = true,
        controls = {
            ui.button{id = 'flash', text = 'Hit the bat', onClick = function()
                self.flash = 1
            end},
            ui.button{id = 'palette', text = 'Next team', onClick = function() self:nextPalette() end},
        },
    }
end

function Materials:nextPalette()
    self.palette = self.palette % #Materials.palettes + 1
    self.materials.palette:set('target_colors', Materials.colors(Materials.palettes[self.palette]))
end

function Materials:update(dt)
    Materials.super.update(self, dt)
    local time = haylen.elapsed()
    -- The bat takes a hit on its own every two seconds, so the flash shows without a press.
    if self:pressed() or math.floor(time / 2) ~= math.floor((time - dt) / 2) then
        self.flash = 1
    end
    self.flash = math.max(0, self.flash - dt * 3)

    local materials = self.materials
    materials.dissolve:set('amount', 0.1 + 0.6 * (0.5 - 0.5 * math.cos(time * 0.8)))
    materials.outline:set('thickness', 6 + math.sin(time * 4) * 2)
    materials.flash:set('amount', self.flash)
    materials.wave:set('time', time)
    materials.hologram:set('time', time)
    materials.rainbow:set('time', time)
    local blocks = 10 + (math.sin(time) + 1) * 40
    materials.pixelate:set('blocks', {blocks, blocks})
    self:status(string.format('The crate is %d%% burned, the planet is %d blocks wide, and the hero wears team %d.', math.floor(materials.dissolve:get('amount') * 100), math.floor(blocks), self.palette))
end

function Materials:draw(area)
    local idle = math.floor(haylen.elapsed() * 6) % 4
    for index, entry in ipairs(Materials.shown) do
        local cell = self.cells[index]
        self:drawCard(cell, entry[1], entry[4])
        local options = {material = self.materials[entry[3]], blend = entry[5]}
        if entry[2] == 'walker' then
            options.source = {idle * 256, 0, 256, 256}
        elseif entry[2] == 'dragon' or entry[2] == 'bat' then
            options.source = {math.floor(haylen.elapsed() * (entry[2] == 'bat' and 8 or 3)) % 2 * 256, 0, 256, 256}
        end
        self:drawPicture(cell, entry[2], cell.size * (entry[2] == 'planet' and 0.95 or 1.1), options)
    end
end

return Materials
