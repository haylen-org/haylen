-- The island the camera tests look at: a meadow of painted tiles ringed by a beach and the sea, with a pond, a sandy clearing and a dirt road, a cottage, a shrine and woods that thicken toward the coast, baked once and drawn by every camera of a test.
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local Terrain = require('categories.sprites.terrain')

local World = haylen.class('World')

World.bounds = m.rect(-2400, -1400, 4800, 2800)
World.cell = 100
-- The coast is a rounded rectangle this far inside the bounds, and the painted sea reaches this far past them.
World.coast = {inset = 240, radius = 520, beach = 110}
World.sea = 600

-- Visibility bits of the draws: the painted ground and the details make the main views, and the map of flat colors and the details make the minimaps, whose masks pick their bits.
World.terrain = 1
World.details = 2
World.map = 4
World.view = World.terrain | World.details
World.mapColors = {[Terrain.grass] = '#FF67B744', [Terrain.sand] = '#FFF3D58D', [Terrain.path] = '#FFB07843', [Terrain.water] = '#FF2FB3C4'}
World.seaColor = '#FF2AA6BC'

-- The art is painted in high definition and the cameras zoom far out, so every image filters linearly with mipmaps.
World.textureOptions = Terrain.textureOptions

World.pond = {x = 600, y = -380, radiusX = 560, radiusY = 360}
World.clearing = {x = -1350, y = 820, radiusX = 420, radiusY = 250}
World.road = {{-2500, 260}, {-1800, 330}, {-1050, 170}, {-350, 230}, {250, 500}, {800, 640}, {1400, 560}, {2000, 320}, {2500, 280}}
World.lane = {{-1050, 170}, {-980, -280}}
World.pathWidth = 70

-- Places the tests name: the cottage, the shrine on the road and the pond.
World.cottage = {x = -980, y = -330}
World.shrine = {x = 820, y = 470}

-- The props of the island: the texture and area of each in the images of the package, the size it draws at, the point it stands on as a fraction of its height, and the radius of its foot that the walkers go around, if any.
World.textures = {'tiled/tiles/props.png', 'tiled/tiles/terrain.png', 'camera/props.png'}
World.kinds = {
    tree = {texture = 1, source = {2, 2, 128, 256}, width = 120, height = 240, pivotY = 0.94, solid = 24, shadow = 54},
    pine = {texture = 1, source = {134, 2, 128, 256}, width = 116, height = 232, pivotY = 0.94, solid = 22, shadow = 46},
    lamp = {texture = 1, source = {266, 2, 128, 256}, width = 96, height = 192, pivotY = 0.95, solid = 12, shadow = 26},
    bush = {texture = 2, source = {266, 266, 128, 128}, width = 84, height = 84, pivotY = 0.86, solid = 26, shadow = 36},
    rock = {texture = 2, source = {398, 266, 128, 128}, width = 96, height = 96, pivotY = 0.86, solid = 32, shadow = 42},
    chest = {texture = 2, source = {662, 266, 128, 128}, width = 72, height = 72, pivotY = 0.9, solid = 24, shadow = 30},
    crate = {texture = 2, source = {530, 266, 128, 128}, width = 72, height = 72, pivotY = 0.9, solid = 24, shadow = 30},
    flag = {texture = 2, source = {926, 266, 128, 128}, width = 80, height = 80, pivotY = 0.9, solid = 8, shadow = 18},
    shrine = {texture = 3, source = {6, 6, 233, 292}, width = 198, height = 248, pivotY = 0.95, solid = 80, shadow = 100},
    cottage = {texture = 3, source = {251, 6, 280, 279}, width = 280, height = 279, pivotY = 0.94, solid = 110, shadow = 130},
    well = {texture = 3, source = {543, 6, 216, 272}, width = 151, height = 190, pivotY = 0.95, solid = 52, shadow = 64},
    signpost = {texture = 3, source = {771, 6, 205, 256}, width = 102, height = 128, pivotY = 0.94, solid = 12, shadow = 30},
    stump = {texture = 3, source = {6, 310, 313, 224}, width = 156, height = 112, pivotY = 0.88, solid = 44, shadow = 62},
    log = {texture = 3, source = {331, 310, 320, 208}, width = 160, height = 104, pivotY = 0.86, solid = 54, shadow = 72},
    mushrooms = {texture = 3, source = {663, 310, 252, 218}, width = 106, height = 92, pivotY = 0.9, shadow = 40},
    flowers = {texture = 3, source = {6, 546, 267, 246}, width = 120, height = 111, pivotY = 0.9, solid = 34, shadow = 50},
    reeds = {texture = 3, source = {285, 546, 226, 257}, width = 102, height = 116, pivotY = 0.92},
    lilies = {texture = 3, source = {523, 546, 274, 167}, width = 123, height = 75, pivotY = 0.5, flat = true},
    barrel = {texture = 3, source = {809, 546, 182, 225}, width = 82, height = 101, pivotY = 0.92, solid = 28, shadow = 34},
    campfire = {texture = 3, source = {6, 815, 233, 231}, width = 140, height = 139, pivotY = 0.86, solid = 48, shadow = 60},
}

function World.texture(path)
    return assets.texture(path, World.textureOptions)
end

function World:init()
    local bounds, cell, sea = World.bounds, World.cell, World.sea
    self.noise = m.noise(5)
    self.ground, self.edges = Terrain.bake((bounds.width + sea * 2) // cell, (bounds.height + sea * 2) // cell, cell, bounds.x - sea, bounds.y - sea, function(column, row)
        return self:kindAt(bounds.x - sea + column * cell, bounds.y - sea + row * cell)
    end)

    local textures = {}
    for index, path in ipairs(World.textures) do
        textures[index] = World.texture(path)
    end
    self:chart()
    self.props = graphics2d.newSpriteBatch(textures)
    self.flats = graphics2d.newSpriteBatch(textures)
    self.placed = {}
    self:plant()
    self:shade()
end

-- Lays out the map of the minimaps: a round spot of flat color on every corner of the island, kept as points of each kind of ground, which merge into smooth shapes at the size of a minimap.
function World:chart()
    local bounds, cell = World.bounds, World.cell
    local spots = {[Terrain.grass] = {}, [Terrain.sand] = {}, [Terrain.path] = {}, [Terrain.water] = {}}
    for row = 0, bounds.height // cell do
        for column = 0, bounds.width // cell do
            local x, y = bounds.x + column * cell, bounds.y + row * cell
            local list = spots[self:kindAt(x, y)]
            if list then
                list[#list + 1] = x
                list[#list + 1] = y
            end
        end
    end
    self.spots = {}
    for kind, list in pairs(spots) do
        local points = collections.newFloatBuffer(#list)
        for index, value in ipairs(list) do
            points[index] = value
        end
        self.spots[kind] = points
    end
end

-- Bakes a soft shadow under every prop that casts one into a static batch, from one image of a radial gradient.
function World:shade()
    local image = graphics.newImage(64, 64)
    image:fillGradient({kind = 'radial', stops = {{0, '#70102010'}, {0.55, '#48102010'}, {1, '#00102010'}}})
    local shadows = graphics2d.newSpriteBatch(graphics.newTexture(image, {filter = 'linear'}))
    for _, prop in ipairs(self.placed) do
        local shadow = prop.spec.shadow
        if shadow then
            shadows:add({x = prop.x, y = prop.y, width = shadow * 2.3, height = shadow * 0.8})
        end
    end
    self.shadows = shadows:bake()
end

-- Returns the kind of ground at a point: the sea past the coast and a beach along it, water in the pond, sand in the clearing, dirt on the road and grass elsewhere, with edges that wander with noise.
function World:kindAt(x, y)
    local wobble = self.noise:fractal(x / 260, y / 260, 3, 2, 0.5) * 0.22
    local inland = World.inland(x, y) + wobble * 260
    if inland < 0 then
        return Terrain.water
    end
    if inland < World.coast.beach then
        return Terrain.sand
    end
    local pond, clearing = World.pond, World.clearing
    if ((x - pond.x) / pond.radiusX) ^ 2 + ((y - pond.y) / pond.radiusY) ^ 2 + wobble < 1 then
        return Terrain.water
    end
    if World.nearPath(x, y, World.pathWidth + wobble * 120) then
        return Terrain.path
    end
    if ((x - clearing.x) / clearing.radiusX) ^ 2 + ((y - clearing.y) / clearing.radiusY) ^ 2 + wobble < 1 then
        return Terrain.sand
    end
    return Terrain.grass
end

-- Returns how far a point lies inside the coast, negative out at sea.
function World.inland(x, y)
    local coast, bounds = World.coast, World.bounds
    local halfWidth, halfHeight = bounds.width / 2 - coast.inset - coast.radius, bounds.height / 2 - coast.inset - coast.radius
    local centerX, centerY = bounds.x + bounds.width / 2, bounds.y + bounds.height / 2
    local dx, dy = math.max(math.abs(x - centerX) - halfWidth, 0), math.max(math.abs(y - centerY) - halfHeight, 0)
    return coast.radius - math.sqrt(dx * dx + dy * dy)
end

-- Tells whether a point lies within a distance of the road or the lane to the cottage.
function World.nearPath(x, y, distance)
    for _, line in ipairs({World.road, World.lane}) do
        for index = 1, #line - 1 do
            if m.distanceToSegment({line[index], line[index + 1]}, {x, y}) < distance then
                return true
            end
        end
    end
    return false
end

-- Adds a prop of a kind that stands at a point.
function World:place(kind, x, y, flip)
    local spec = World.kinds[kind]
    local batch = spec.flat and self.flats or self.props
    batch:add({x = x, y = y, width = spec.width, height = spec.height, pivotY = spec.pivotY, texture = spec.texture, source = spec.source, flipHorizontal = flip})
    self.placed[#self.placed + 1] = {kind = kind, x = x, y = y, spec = spec}
end

-- Tells whether a prop may stand at a point: on grass, off the road and away from the places the tests name.
function World:free(x, y, margin)
    if self:kindAt(x, y) ~= Terrain.grass or World.nearPath(x, y, World.pathWidth + margin) then
        return false
    end
    for _, spot in ipairs({World.cottage, World.shrine, World.clearing}) do
        if (x - spot.x) ^ 2 + (y - spot.y) ^ 2 < (260 + margin) ^ 2 then
            return false
        end
    end
    return true
end

-- Plants the woods, thick along the edges and sparse inside, then scatters bushes and rocks and sets the props of the cottage, the shrine, the clearing and the road.
function World:plant()
    local bounds = World.bounds
    local inner = m.rect(bounds.x + 380, bounds.y + 320, bounds.width - 760, bounds.height - 640)
    local random = m.random(12)
    local trees = m.poissonDisk({area = bounds, minimumDistance = 120, maximumDistance = 520, seed = 3, distance = function(point)
        return inner:contains(point) and 520 or 120
    end, accept = function(point)
        return self:free(point.x, point.y, 70)
    end})
    for _, point in ipairs(trees) do
        self:place(random:nextFloat() < 0.35 and 'pine' or 'tree', point.x, point.y, random:nextFloat() < 0.5)
    end
    local scattered = m.poissonDisk({area = inner, minimumDistance = 260, seed = 8, accept = function(point)
        return self:free(point.x, point.y, 40)
    end})
    for _, point in ipairs(scattered) do
        local roll = random:nextFloat()
        if roll < 0.45 then
            self:place('bush', point.x, point.y, random:nextFloat() < 0.5)
        elseif roll < 0.75 then
            self:place('rock', point.x, point.y, random:nextFloat() < 0.5)
        end
    end

    for index = 1, 26 do
        local angle = index / 26 * math.pi * 2 + random:range(-0.08, 0.08)
        local point = self:shore(angle, 1.06)
        if self:kindAt(point.x, point.y) ~= Terrain.water and index % 3 ~= 0 then
            self:place('reeds', point.x, point.y, random:nextFloat() < 0.5)
        end
    end
    for index = 1, 7 do
        local point = self:shore(index / 7 * math.pi * 2 + 0.4, random:range(0.25, 0.65))
        self:place('lilies', point.x, point.y, random:nextFloat() < 0.5)
    end
    local small = m.poissonDisk({area = inner, minimumDistance = 420, seed = 21, accept = function(point)
        return self:free(point.x, point.y, 60)
    end})
    for index, point in ipairs(small) do
        self:place(({'mushrooms', 'flowers', 'stump', 'flowers'})[index % 4 + 1], point.x, point.y, random:nextFloat() < 0.5)
    end
    self:settle()
end

-- Returns a point at a share of the radii of the pond along an angle.
function World:shore(angle, share)
    local pond = World.pond
    return m.vec2(pond.x + math.cos(angle) * pond.radiusX * share, pond.y + math.sin(angle) * pond.radiusY * share)
end

-- Sets the props of the places the tests name: the cottage with its well and barrels, the signpost at the fork, the shrine on the road, the lamps along it and the camp in the clearing.
function World:settle()
    local cottage, shrine, clearing = World.cottage, World.shrine, World.clearing
    self:place('cottage', cottage.x, cottage.y)
    self:place('well', cottage.x + 300, cottage.y + 70)
    self:place('barrel', cottage.x - 210, cottage.y + 30)
    self:place('barrel', cottage.x - 150, cottage.y + 70)
    self:place('crate', cottage.x - 260, cottage.y + 90)
    self:place('signpost', -1180, 330)
    self:place('shrine', shrine.x, shrine.y)
    self:place('flag', shrine.x + 160, shrine.y + 30)
    self:place('flag', shrine.x - 160, shrine.y + 30, true)
    for _, x in ipairs({-500, 300, 1250, 1950}) do
        self:place('lamp', x, World.roadY(x) - 105)
    end
    self:place('campfire', clearing.x, clearing.y)
    self:place('log', clearing.x - 190, clearing.y - 40)
    self:place('log', clearing.x + 200, clearing.y + 50, true)
    self:place('stump', clearing.x + 40, clearing.y + 170)
    self:place('chest', clearing.x - 120, clearing.y + 150)
end

-- Returns the y of the road at an x, along its segments.
function World.roadY(x)
    local road = World.road
    for index = 1, #road - 1 do
        local a, b = road[index], road[index + 1]
        if x >= a[1] and x <= b[1] then
            return m.lerp(a[2], b[2], (x - a[1]) / (b[1] - a[1]))
        end
    end
    return road[#road][2]
end

-- Tells whether a walker may stand at a point: on land, inside the island and off the foot of every prop.
function World:walkable(x, y)
    local bounds = World.bounds
    if x < bounds.x + 60 or x > bounds:right() - 60 or y < bounds.y + 60 or y > bounds:bottom() - 30 then
        return false
    end
    if self:kindAt(x, y) == Terrain.water then
        return false
    end
    for _, prop in ipairs(self.placed) do
        local radius = prop.spec.solid
        if radius and (x - prop.x) ^ 2 + ((y - prop.y) * 1.6) ^ 2 < radius * radius then
            return false
        end
    end
    return true
end

-- Draws the world into the active canvas, which sorts by y: the ground below everything, the shadows on it, and the props among the walkers by the y they stand on, and the map of flat colors. The ground takes the terrain bit, the map the map bit and the rest the details bit, so the mask of each canvas picks what it shows.
function World:draw()
    local bounds = World.bounds
    graphics2d.drawRect({bounds.x - 8000, bounds.y - 8000, bounds.width + 16000, bounds.height + 16000}, World.seaColor, {layer = -4, visibility = World.terrain | World.map})
    graphics2d.drawStatic(self.ground, 0, 0, {layer = -3, visibility = World.terrain})
    graphics2d.drawStatic(self.edges, 0, 0, {layer = -2, visibility = World.terrain})
    for kind, points in pairs(self.spots) do
        graphics2d.drawPoints(points, {radius = World.cell * 0.75, color = World.mapColors[kind], layer = kind - 3, visibility = World.map})
    end
    graphics2d.drawStatic(self.shadows, 0, 0, {layer = -1, visibility = World.details})
    self.flats:draw({layer = -1, visibility = World.details})
    self.props:draw({layer = 1, visibility = World.details})
end

return World
