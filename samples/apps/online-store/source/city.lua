-- The made-up city the delivery vans drive through: a grid of blocks with streets, avenues, a river with bridges, parks and buildings, generated from a seed, with the shortest routes between its corners and the drawing of the map with graphics2d.
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
    ground = '#FFEDE7E1',
    road = '#FFFFFFFF',
    avenue = '#FFFFEBD6',
    block = '#FFF8F4EF',
    buildings = {'#FFE9E0D8', '#FFE2D8CF', '#FFEEE6DE', '#FFDCD1C7'},
    roof = '#FFD2C5B9',
    park = '#FFD3E8C8',
    tree = '#FF92C57E',
    treeDark = '#FF77AE63',
    water = '#FFAED8EC',
    waterEdge = '#FF8CC6E1',
    bridge = '#FFFFFFFF',
    label = '#FF8F857C',
}

City.dark = {
    ground = '#FF16171F',
    road = '#FF2B2D3A',
    avenue = '#FF383A4A',
    block = '#FF1B1C26',
    buildings = {'#FF23242F', '#FF262734', '#FF20212B', '#FF2A2B38'},
    roof = '#FF31323F',
    park = '#FF1D2B22',
    tree = '#FF2E4A35',
    treeDark = '#FF243C2A',
    water = '#FF1A3040',
    waterEdge = '#FF213A4D',
    bridge = '#FF383A4A',
    label = '#FF757789',
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

-- The point of the corner at a column and a row of the street grid.
function City:corner(column, row)
    return self.columns[column], self.rows[row]
end

-- Whether a road joins two neighboring corners: no road runs along the river, and only avenues cross it on bridges.
function City:connected(column, row, nextColumn, nextRow)
    if row == nextRow then
        return row ~= City.riverRow
    end
    if row == City.riverRow or nextRow == City.riverRow then
        return self:isAvenue(column)
    end
    return true
end

-- Returns the corners of the shortest route between two corners as a flat list of x and y pairs, found with A* over the street grid. A corner in the river moves to the bank above it.
function City:route(fromColumn, fromRow, toColumn, toRow)
    if fromRow == City.riverRow then
        fromRow = fromRow - 1
    end
    if toRow == City.riverRow then
        toRow = toRow - 1
    end
    local count = City.blocks + 1
    local function key(column, row)
        return row * count + column
    end
    local function estimate(column, row)
        local x, y = self:corner(column, row)
        local goalX, goalY = self:corner(toColumn, toRow)
        return math.abs(goalX - x) + math.abs(goalY - y)
    end
    local start, goal = key(fromColumn, fromRow), key(toColumn, toRow)
    local open, cost, previous, closed = {start}, {[start] = 0}, {}, {}
    local score = {[start] = estimate(fromColumn, fromRow)}
    local steps = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}
    while #open > 0 do
        local best = 1
        for index = 2, #open do
            if score[open[index]] < score[open[best]] then
                best = index
            end
        end
        local current = table.remove(open, best)
        if current == goal then
            break
        end
        closed[current] = true
        local column, row = current % count, current // count
        local x, y = self:corner(column, row)
        for _, step in ipairs(steps) do
            local nextColumn, nextRow = column + step[1], row + step[2]
            if nextColumn >= 0 and nextColumn < count and nextRow >= 0 and nextRow < count and self:connected(column, row, nextColumn, nextRow) then
                local next = key(nextColumn, nextRow)
                if not closed[next] then
                    local nextX, nextY = self:corner(nextColumn, nextRow)
                    local tentative = cost[current] + math.abs(nextX - x) + math.abs(nextY - y)
                    if cost[next] == nil or tentative < cost[next] then
                        if cost[next] == nil then
                            open[#open + 1] = next
                        end
                        cost[next] = tentative
                        score[next] = tentative + estimate(nextColumn, nextRow)
                        previous[next] = current
                    end
                end
            end
        end
    end
    local corners = {}
    local node = goal
    while node do
        table.insert(corners, 1, node)
        node = previous[node]
    end
    local points = {}
    for index, corner in ipairs(corners) do
        local column, row = corner % count, corner // count
        local x, y = self:corner(column, row)
        -- Corners in a straight line add nothing to the drawing, so only the turns stay.
        local after = corners[index + 1]
        local before = corners[index - 1]
        if not before or not after or (before % count == column) ~= (column == after % count) then
            points[#points + 1] = x
            points[#points + 1] = y
        end
    end
    return points, cost[goal] or 0
end

-- Returns the length of a route and the point at a distance along it.
function City.routeLength(points)
    local length = 0
    for index = 3, #points, 2 do
        length = length + math.abs(points[index] - points[index - 2]) + math.abs(points[index + 1] - points[index - 1])
    end
    return length
end

function City.pointAlong(points, distance)
    for index = 3, #points, 2 do
        local x1, y1, x2, y2 = points[index - 2], points[index - 1], points[index], points[index + 1]
        local length = math.abs(x2 - x1) + math.abs(y2 - y1)
        if distance <= length then
            local share = length > 0 and distance / length or 0
            return x1 + (x2 - x1) * share, y1 + (y2 - y1) * share, x2 - x1, y2 - y1
        end
        distance = distance - length
    end
    return points[#points - 1], points[#points], 0, 0
end

-- Returns the points of a route up to a distance, for the part a courier still has to ride.
function City.routeFrom(points, distance)
    local rest = {}
    local x, y = City.pointAlong(points, distance)
    rest[1], rest[2] = x, y
    local walked = 0
    for index = 3, #points, 2 do
        walked = walked + math.abs(points[index] - points[index - 2]) + math.abs(points[index + 1] - points[index - 1])
        if walked > distance then
            rest[#rest + 1] = points[index]
            rest[#rest + 1] = points[index + 1]
        end
    end
    return rest
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
