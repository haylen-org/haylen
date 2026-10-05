-- The flight over the world. The camera flies by itself until the player takes it: the arrows, WASD, a stick or a drag move it, and the wheel, a pinch, the triggers or Q and E zoom. The panel zooms, sets the time the chunk builds may take each frame, outlines the chunks and starts a new world.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local jobs = require('haylen.jobs')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Chunks = require('chunks')
local Hud = require('hud')
local Terrain = require('terrain')
local Tour = require('tour')

local Flight = haylen.class('Flight', scene.Scene)

Flight.startZoom = 0.5
Flight.minimumZoom = 0.03
Flight.maximumZoom = 2
Flight.zoomRate = 6
Flight.zoomSpeed = 1.4
Flight.flySpeed = 360
Flight.moveSpeed = 900
Flight.turnRate = 0.7
Flight.budgets = {{id = '2', text = '2 ms'}, {id = '4', text = '4 ms'}, {id = '8', text = '8 ms'}, {id = '16', text = '16 ms'}}
Flight.firstSeed = 2024
Flight.actions = {actions = {
    {name = 'move', type = 'vector', up = {'key:up', 'key:w'}, down = {'key:down', 'key:s'}, left = {'key:left', 'key:a'}, right = {'key:right', 'key:d'}, bindings = {'stick:left'}},
    {name = 'zoom', type = 'axis', positive = {'key:e', 'key:equal', 'key:keypadAdd', 'axis:rightTrigger+'}, negative = {'key:q', 'key:minus', 'key:keypadSubtract', 'axis:leftTrigger+'}},
    {name = 'uiFocus', type = 'button', bindings = {'button:back', 'key:pause'}},
}}

function Flight:load(context)
    context:progress(0, 'Drawing the terrain')
    local texture, failure = assets.vectorImage('terrain/terrain.svg'):rasterize(2):await()
    if not texture then
        error(failure, 0)
    end
    self.atlas = texture
end

function Flight:enter()
    input.loadActions(Flight.actions)
    jobs.setBudget(4)
    self.chunks = Chunks.new(self, self.atlas, Flight.firstSeed)
    self.seed = Flight.firstSeed
    self.camera = graphics2d.newCamera()
    local half = Chunks.extent * Terrain.chunkSize
    self.camera.limits = {-half, -half, half * 2, half * 2}
    self.camera.minZoom = Flight.minimumZoom
    self.camera.maxZoom = Flight.maximumZoom
    self.camera.zoom = {Flight.startZoom, Flight.startZoom}
    self.zoomTarget = Flight.startZoom
    self.heading = 0.4
    self.time = 0
    self.autopilot = true
    self.outlines = true
    self.pinchScale = 1
    self.mouseDrag = false
    self.tilesDrawn, self.decorationsDrawn = 0, 0
    self.turns = m.noise(5)
    self.hud = Hud.new(self, {
        title = 'Streamed Map',
        caption = 'A world of 16,384 by 16,384 tiles from a seed, streamed in chunks of 64 by 64 tiles.',
        rows = {
            {id = 'loaded', label = 'Loaded chunks'},
            {id = 'building', label = 'Chunks building and waiting'},
            {id = 'tiles', label = 'Tiles drawn'},
            {id = 'decorations', label = 'Trees and rocks drawn'},
            {id = 'baked', label = 'Baked sprites'},
            {id = 'memory', label = 'Lua memory'},
            {id = 'zoom', label = 'Zoom'},
        },
        controls = {
            ui.toggle{id = 'autopilot', text = 'Autopilot', checked = true, onChange = function(event) self.autopilot = event.checked end},
            ui.row{gap = 12,
                ui.button{id = 'zoomOut', text = 'Zoom out', grow = 1, onClick = function() self:zoomBy(0.5) end},
                ui.button{id = 'zoomIn', text = 'Zoom in', grow = 1, onClick = function() self:zoomBy(2) end},
            },
            ui.label{text = 'Build time per frame', font = 'caption', color = 'textMuted'},
            ui.segmentedControl{id = 'budget', items = Flight.budgets, selected = '4', onChange = function(event) jobs.setBudget(tonumber(event.value)) end},
            ui.toggle{id = 'outlines', text = 'Chunk outlines', checked = true, onChange = function(event) self.outlines = event.checked end},
            ui.button{id = 'newWorld', text = 'New world', onClick = function() self:newWorld() end},
        },
        hint = 'The arrows, WASD, the left stick or a drag fly the camera, and the wheel, a pinch, the triggers, Q and E or plus and minus zoom. Tab, the View button or Play/Pause moves the focus between the map and the panel.',
        focus = 'stage',
    })
    if haylen.platform == 'headless' then
        Tour.start(self, self:tourSteps())
    end
end

-- The steps of the automatic run: the flight at several zooms, every budget and a new world.
function Flight:tourSteps()
    local function step(change)
        return function()
            change()
            return string.format('zoom %.2f with %d chunks loaded and %d building', self.zoomTarget, self.chunks.loadedCount, self.chunks.buildCount)
        end
    end
    return {
        step(function() end),
        step(function() self:zoomBy(0.25) end),
        step(function() jobs.setBudget(8) end),
        step(function() self:zoomBy(0.5) end),
        step(function() self.outlines = false end),
        step(function() self:zoomBy(8) end),
        step(function() self:newWorld() end),
        step(function() jobs.setBudget(2) end),
        step(function() self.autopilot = false end),
    }
end

function Flight:zoomBy(factor)
    self.zoomTarget = m.clamp(self.zoomTarget * factor, Flight.minimumZoom, Flight.maximumZoom)
end

function Flight:newWorld()
    self.seed = self.seed + 1
    self.chunks:reseed(self.seed)
end

function Flight:takeControl()
    if self.autopilot then
        self.autopilot = false
        self.hud:set('autopilot', 'checked', false)
    end
end

-- The autopilot flies ahead and turns by a slow noise, and back toward the middle of the world once it nears an edge.
function Flight:fly(dt)
    local camera = self.camera
    local half = Chunks.extent * Terrain.chunkSize
    local turn = self.turns:simplex(self.time * 0.07, 3) * Flight.turnRate
    local x, y = camera.x, camera.y
    if math.abs(x) > half * 0.8 or math.abs(y) > half * 0.8 then
        local home = math.atan(-y, -x)
        turn = turn + m.wrapAngle(home - self.heading) * 0.8
    end
    self.heading = self.heading + turn * dt
    local speed = Flight.flySpeed / camera.zoom.x * dt
    camera.x, camera.y = x + math.cos(self.heading) * speed, y + math.sin(self.heading) * speed
end

-- The arrows, a stick and a drag move the camera, and the wheel, a pinch and the zoom action zoom it. Any of them takes the camera from the autopilot.
function Flight:steer(stage, dt)
    local camera = self.camera
    local zoom = camera.zoom.x
    local dx, dy = input.vector('move')
    if dx ~= 0 or dy ~= 0 then
        self:takeControl()
        camera.x, camera.y = camera.x + dx * Flight.moveSpeed / zoom * dt, camera.y + dy * Flight.moveSpeed / zoom * dt
    end

    -- A drag counts only when it started on the map, so a press on the panel never moves the camera.
    local touches = input.touches()
    if input.mousePressed('left') then
        local x, y = input.mousePosition()
        self.mouseDrag = #touches == 0 and stage:contains({x, y}) and not ui.usingPointer()
    elseif not input.mouseDown('left') then
        self.mouseDrag = false
    end
    local dragging = false
    if #touches == 1 and stage:contains({touches[1].startX, touches[1].startY}) then
        dragging, dx, dy = true, touches[1].dx, touches[1].dy
    elseif self.mouseDrag then
        dragging, dx, dy = true, input.mouseDelta()
    end
    if dragging and (dx ~= 0 or dy ~= 0) then
        self:takeControl()
        camera.x, camera.y = camera.x - dx / zoom, camera.y - dy / zoom
    end

    for _, gesture in ipairs(input.gestures()) do
        if gesture.type == 'pinch' then
            camera:zoomAt(gesture.scale / self.pinchScale, gesture.x, gesture.y)
            self.pinchScale = gesture.scale
            self.zoomTarget = camera.zoom.x
        end
    end
    if #touches < 2 then
        self.pinchScale = 1
    end
    local zoomInput = input.value('zoom')
    if zoomInput ~= 0 then
        self:zoomBy(math.exp(zoomInput * Flight.zoomSpeed * dt))
    end
    camera:clampToLimits()
end

function Flight:event(event)
    if event.type == 'mouseScroll' and self.stage and not ui.usingPointer() then
        local x, y = input.mousePosition()
        if self.stage:contains({x, y}) then
            self.camera:zoomAt(event.scrollY > 0 and 1.2 or 1 / 1.2, x, y)
            self.zoomTarget = self.camera.zoom.x
        end
    end
end

function Flight:update(dt)
    local stage = self.hud:stage()
    if not stage then
        return
    end
    self.stage = stage
    self.time = self.time + dt
    local camera = self.camera
    camera.viewport = stage
    self:steer(stage, dt)
    if self.autopilot then
        self:fly(dt)
    end
    local zoom = camera.zoom.x
    if math.abs(zoom - self.zoomTarget) > 1e-4 then
        local eased = math.exp(m.lerp(math.log(zoom), math.log(self.zoomTarget), m.dampFactor(Flight.zoomRate, dt)))
        camera.zoom = {eased, eased}
    end
    camera:clampToLimits()
    self.chunks:stream(camera:visibleBounds())

    if self.hud:update(dt) then
        local chunks = self.chunks
        self.hud:show('loaded', Hud.count(chunks.loadedCount))
        self.hud:show('building', string.format('%s and %s', Hud.count(chunks.buildCount), Hud.count(chunks.waiting)))
        self.hud:show('tiles', Hud.count(self.tilesDrawn))
        self.hud:show('decorations', Hud.count(self.decorationsDrawn))
        self.hud:show('baked', Hud.count(chunks.bakedSprites))
        self.hud:show('memory', string.format('%.1f MB', self.hud.stats.memory.lua / 1048576))
        self.hud:show('zoom', string.format('%.2f', camera.zoom.x))
    end
end

function Flight:render()
    if not self.stage then
        return
    end
    graphics2d.beginWorld(self.camera)
    local bounds = self.camera:visibleBounds()
    self.tilesDrawn, self.decorationsDrawn = self.chunks:draw(bounds)
    if self.outlines then
        self.chunks:drawOutlines(bounds, self.time)
    end
end

return Flight
