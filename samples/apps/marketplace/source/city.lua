-- The made-up city where the market meets: a grid of blocks with streets, avenues, a river with bridges, parks and buildings, generated from a seed, and the drawing of the map with graphics2d.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Random = require('random')

local City = haylen.class('City')

City.blocks = 16
City.blockSize = 150
City.street = 22
City.avenue = 40
City.riverRow = 9
City.riverWidth = 120
City.avenueEvery = 4

City.light = {
    ground = '#FFE6ECE9',
    road = '#FFFFFFFF',
    avenue = '#FFFFF3D6',
    block = '#FFF1F5F3',
    buildings = {'#FFDCE5E1', '#FFD4DED9', '#FFE2E9E6', '#FFCFD9D4'},
    roof = '#FFC2CEC8',
    park = '#FFC9E7C4',
    tree = '#FF86C77A',
    treeDark = '#FF6AAF5E',
    water = '#FFA6D9E6',
    waterEdge = '#FF86C8DA',
    bridge = '#FFFFFFFF',
    label = '#FF7F908A',
}

City.dark = {
    ground = '#FF141D1A',
    road = '#FF26332E',
    avenue = '#FF34413A',
    block = '#FF18221F',
    buildings = {'#FF1F2B27', '#FF22302B', '#FF1C2724', '#FF263430'},
    roof = '#FF2C3B35',
    park = '#FF1E3324',
    tree = '#FF2F5236',
    treeDark = '#FF24432B',
    water = '#FF163844',
    waterEdge = '#FF1D4757',
    bridge = '#FF34413A',
    label = '#FF6F827B',
}

function City:init(seed)
    self.random = Random(seed)
    self.columns = self:streetLines(false)
    self.rows = self:streetLines(true)
    self.width = self.columns[City.blocks]
    self.height = self.rows[City.blocks]
    self:buildBlocks()
end

-- The middle lines of the streets across one axis, where the rows leave room for the river.
function City:streetLines(vertical)
    local lines = {}
    local position = 0
    for index = 0, City.blocks do
        local width = self:streetWidth(index, vertical)
        if index > 0 then
            position = position + City.blockSize + width / 2
        end
        lines[index] = position
        position = position + width / 2
    end
    return lines
end

-- Every fourth street is an avenue, and the river takes the place of one street.
function City:streetWidth(index, vertical)
    if vertical and index == City.riverRow then
        return City.riverWidth
    end
    return index % City.avenueEvery == 0 and City.avenue or City.street
end

function City:isAvenue(index)
    return index % City.avenueEvery == 0
end

-- The rectangle of the block at a column and a row, between the streets around it.
function City:blockRect(column, row)
    local left = self.columns[column] + self:streetWidth(column) / 2
    local right = self.columns[column + 1] - self:streetWidth(column + 1) / 2
    local top = self.rows[row] + self:streetWidth(row, true) / 2
    local bottom = self.rows[row + 1] - self:streetWidth(row + 1, true) / 2
    return left, top, right - left, bottom - top
end

function City:buildBlocks()
    self.parcels = {}
    self.parks = {}
    self.trees = {}
    local random = self.random
    for row = 0, City.blocks - 1 do
        for column = 0, City.blocks - 1 do
            local x, y, width, height = self:blockRect(column, row)
            if random:chance(0.09) then
                self.parks[#self.parks + 1] = {x, y, width, height}
                for _ = 1, random:integer(5, 9) do
                    self.trees[#self.trees + 1] = {x + random:number(16, width - 16), y + random:number(16, height - 16), random:number(9, 15)}
                end
            else
                self:buildLots(x, y, width, height)
            end
        end
    end
end

-- Splits a block into two to four buildings with a small gap between them.
function City:buildLots(x, y, width, height)
    local random = self.random
    local gap = 8
    local columns, rows = random:integer(1, 2), random:integer(1, 2)
    local lotWidth = (width - gap * (columns + 1)) / columns
    local lotHeight = (height - gap * (rows + 1)) / rows
    for row = 0, rows - 1 do
        for column = 0, columns - 1 do
            self.parcels[#self.parcels + 1] = {
                x + gap + column * (lotWidth + gap), y + gap + row * (lotHeight + gap), lotWidth, lotHeight,
                random:integer(1, 4),
            }
        end
    end
end

-- Draws the city in world units with the colors of `palette`, inside the world canvas that the caller began.
function City:draw(palette, visible)
    local width, height = self.width, self.height
    graphics2d.drawRect({-600, -600, width + 1200, height + 1200}, palette.ground)
    for index = 0, City.blocks do
        local column, row = self.columns[index], self.rows[index]
        local street = self:streetWidth(index)
        local color = self:isAvenue(index) and palette.avenue or palette.road
        graphics2d.drawRect({column - street / 2, -600, street, height + 1200}, color)
        if index ~= City.riverRow then
            graphics2d.drawRect({-600, row - street / 2, width + 1200, street}, color)
        end
    end
    local riverY = self.rows[City.riverRow]
    graphics2d.drawRect({-600, riverY - City.riverWidth / 2, width + 1200, City.riverWidth}, palette.waterEdge)
    graphics2d.drawRect({-600, riverY - City.riverWidth / 2 + 8, width + 1200, City.riverWidth - 16}, palette.water)
    for index = 0, City.blocks, City.avenueEvery do
        local line = self.columns[index]
        graphics2d.drawShape({line - City.avenue / 2 - 6, riverY - City.riverWidth / 2 - 4, City.avenue + 12, City.riverWidth + 8}, {radius = 6, color = palette.bridge, borderWidth = 3, borderColor = palette.roof})
    end
    for _, park in ipairs(self.parks) do
        if self.overlaps(park, visible) then
            graphics2d.drawShape(park, {radius = 14, color = palette.park})
        end
    end
    for _, tree in ipairs(self.trees) do
        graphics2d.drawCircle(tree[1] + 2, tree[2] + 3, tree[3], palette.treeDark)
        graphics2d.drawCircle(tree[1], tree[2], tree[3], palette.tree)
    end
    for _, parcel in ipairs(self.parcels) do
        if self.overlaps(parcel, visible) then
            graphics2d.drawShape(parcel, {radius = 8, color = palette.buildings[parcel[5]], borderWidth = 2, borderColor = palette.roof})
        end
    end
end

function City.overlaps(rect, visible)
    return not visible or (rect[1] < visible.x + visible.width and rect[1] + rect[3] > visible.x and rect[2] < visible.y + visible.height and rect[2] + rect[4] > visible.y)
end

return City
