-- XML with Varn's "xml" module: a small level written in XML, decoded into nodes and drawn from their attributes, the node model encoded back to text, and escaping.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local xml = require('xml')

local Test = require('harness.test')
local VarnTest = require('categories.varn.varn-test')

local Xml = haylen.class('Xml', VarnTest)

Xml.hint = 'The cove under the checks stands where the attributes of the decoded XML put it. Run again decodes it again.'

Xml.level = [[
<level name="Cove">
  <island x="190" y="178" size="170"/>
  <island x="575" y="172" size="124"/>
  <crab x="330" y="190" size="62"/>
  <shell x="660" y="186" size="50"/>
  <boat x="850" y="168" size="138"/>
  <note>The crab guards the shell</note>
</level>]]

-- The props of the cove, one cell of 256 pixels each in the painted sheet, the size of the level the attributes name places in and the height of the stage it draws on.
Xml.sheet = 'varn/cove.png'
Xml.cells = {island = 0, crab = 1, shell = 2, boat = 3}
Xml.world = {1000, 220}
Xml.stageHeight = 250
Xml.sea = {'#FF5CCAD6', '#FF2FB3C4', '#FF1E8FA6'}

Xml.excerpts = {
    {'Decode', [[
local level = xml.decode(Xml.level)
print(level.name, level.attributes.name)
for _, node in ipairs(level.children) do
    if node.attributes then
        print(node.name, tonumber(node.attributes.x))
    else
        print(node.name, node.text)
    end
end]]},
    {'Encode', [[
local text = xml.encode({
    name = 'save', attributes = {slot = '1'},
    children = {{name = 'gold', text = '12'}},
})
local sign = xml.encode({
    name = 'sign', text = 'Fish < 3 & chips',
})
local ok, failure = pcall(xml.decode, '<open>')]]},
}

function Xml:run()
    local level = xml.decode(Xml.level)
    self.shapes = {}
    local note
    for _, node in ipairs(level.children) do
        if node.attributes then
            self.shapes[#self.shapes + 1] = {name = node.name, x = tonumber(node.attributes.x), y = tonumber(node.attributes.y), size = tonumber(node.attributes.size)}
        elseif node.name == 'note' then
            note = node.text
        end
    end
    self:check('decode', 'Decode', level.name == 'level' and level.attributes.name == 'Cove' and #level.children == 6, string.format('The root "%s" named "%s" has %d children, and the cove below stands where their attributes say.', level.name, level.attributes.name, #level.children))
    self:check('text', 'Text of a node', note == 'The crab guards the shell', string.format('The node "note" has no attributes and holds the text "%s".', tostring(note)))

    local text = xml.encode({
        name = 'save', attributes = {slot = '1'},
        children = {{name = 'gold', text = '12'}},
    })
    self:check('encode', 'Encode', text:find('<save slot="1">', 1, true) ~= nil and text:find('<gold>12</gold>', 1, true) ~= nil, 'Wrote ' .. text:gsub('%s+$', ''))

    local sign = xml.encode({
        name = 'sign', text = 'Fish < 3 & chips',
    })
    local back = xml.decode(sign).text
    self:check('escape', 'Escaping', sign:find('Fish &lt; 3 &amp; chips', 1, true) ~= nil and back == 'Fish < 3 & chips', string.format('The text became "Fish &lt; 3 &amp; chips" in the document and "%s" again once decoded.', back))

    local ok, failure = pcall(xml.decode, '<open>')
    self:check('invalid', 'Invalid text', not ok and failure ~= nil, 'The call raised, and "pcall" caught it: ' .. tostring(failure))

    self:check('aliases', 'Aliases', xml.parse('<a/>').name == 'a' and xml.stringify({name = 'b'}):find('<b', 1, true) ~= nil, 'The functions "xml.parse" and "xml.stringify" are the same as "xml.decode" and "xml.encode".')
end

-- Draws a strip of sea with its shallow band and its waves, and every prop of the decoded level at the place and the size of its attributes, with its name on a dark pill under it.
function Xml:draw(area)
    local margin = 16
    local width, height = area.width - margin * 2, area.height - margin * 2
    graphics2d.drawShape({margin, margin, width, height}, {radius = 14, color = Xml.sea[2]})
    graphics2d.drawShape({margin, margin, width, height * 0.42}, {radius = {14, 14, 0, 0}, color = Xml.sea[1], layer = 1})
    graphics2d.drawShape({margin, margin + height - 14, width, 14}, {radius = {0, 0, 14, 14}, color = Xml.sea[3], layer = 1})
    for wave = 0, 7 do
        local x = margin + 40 + wave * width / 8
        local y = margin + height * (wave % 2 == 0 and 0.6 or 0.78)
        graphics2d.drawArc(x, y, 18, 3, math.pi * 1.15, math.pi * 1.85, '#80F2F3FF', {layer = 2})
    end
    if not self.shapes then
        return
    end

    self.cove = self.cove or assets.texture(Xml.sheet, {filter = 'linear'})
    local scale = math.min(width / Xml.world[1], height / Xml.world[2])
    local left = margin + (width - Xml.world[1] * scale) / 2
    for _, shape in ipairs(self.shapes) do
        local x, y, size = left + shape.x * scale, margin + shape.y * scale, shape.size * scale
        graphics2d.draw(self.cove, x, y, {source = {Xml.cells[shape.name] * 256, 0, 256, 256}, width = size, height = size, pivotX = 0.5, pivotY = 1, layer = 3})
        local name = shape.name:sub(1, 1):upper() .. shape.name:sub(2)
        local labelWidth = graphics2d.measureText(nil, name, {size = 16}) + 20
        graphics2d.drawShape({x - labelWidth / 2, y + 4, labelWidth, 24}, {radius = 12, color = '#D91E1B33', layer = 4})
        graphics2d.drawText(nil, name, x, y + 16, {size = 16, color = Test.ink, anchor = {0.5, 0.5}, layer = 5})
    end
end

return Xml
