-- XML with Varn's "xml" module: a small level written in XML, decoded into nodes and drawn from their attributes, the node model encoded back to text, and escaping.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local xml = require('xml')

local Test = require('harness.test')
local VarnTest = require('categories.varn.varn-test')

local Xml = haylen.class('Xml', VarnTest)

Xml.hint = 'The shapes under the checks come from the attributes of the decoded XML. Run again decodes it again.'

Xml.level = [[
<level name="Cove">
  <island x="140" y="90" size="60" color="#FF6FDCA0"/>
  <island x="420" y="80" size="44" color="#FF6FDCA0"/>
  <crab x="190" y="120" size="14" color="#FFFF8A84"/>
  <shell x="440" y="66" size="10" color="#FFF2B23A"/>
  <boat x="680" y="96" size="24" color="#FF8FB0FF"/>
  <note>The crab guards the shell</note>
</level>]]

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
            self.shapes[#self.shapes + 1] = {name = node.name, x = tonumber(node.attributes.x), y = tonumber(node.attributes.y), size = tonumber(node.attributes.size), color = node.attributes.color}
        elseif node.name == 'note' then
            note = node.text
        end
    end
    self:check('decode', 'Decode', level.name == 'level' and level.attributes.name == 'Cove' and #level.children == 6, string.format('The root "%s" named "%s" has %d children, and the shapes below come from their attributes.', level.name, level.attributes.name, #level.children))
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

function Xml:draw(area)
    local top = self.results:draw(area)
    if not self.shapes then
        return
    end
    graphics2d.drawRect({24, top, 860, 190}, '#FF1F3A4D')
    for _, shape in ipairs(self.shapes) do
        graphics2d.drawCircle(24 + shape.x, top + shape.y, shape.size, shape.color, {layer = 1})
        Test.caption(shape.name:sub(1, 1):upper() .. shape.name:sub(2), 24 + shape.x, top + shape.y + shape.size + 4, {size = 16, color = Test.ink, anchor = {0.5, 0}})
    end
end

return Xml
