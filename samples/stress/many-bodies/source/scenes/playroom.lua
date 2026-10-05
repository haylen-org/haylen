-- The playroom: the toy bin seen whole through a camera that fits it into the play area. The panel pours and removes toys a thousand at a time, sets off a blast and shakes the bin, and a click or a tap on the bin sets off a blast there. The wheel, a pinch or the right stick zooms in.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Hud = require('hud')
local ToyBin = require('toy-bin')
local Tour = require('tour')

local Playroom = haylen.class('Playroom', scene.Scene)

Playroom.start = 3000
Playroom.step = 1000
Playroom.maximumZoom = 6
Playroom.zoomSpeed = 1.6
Playroom.chuteSize = {ToyBin.mouth / 0.6, ToyBin.mouth / 0.6 * 0.7}
Playroom.blastTime = 0.45
Playroom.sweepInterval = 0.5
Playroom.insideColor = '#FF2C2346'
Playroom.actions = {actions = {
    {name = 'more', type = 'button', bindings = {'key:equal', 'key:keypadAdd', 'axis:rightTrigger+'}},
    {name = 'fewer', type = 'button', bindings = {'key:minus', 'key:keypadSubtract', 'axis:leftTrigger+'}},
    {name = 'blast', type = 'button', bindings = {'key:b', 'button:west'}},
    {name = 'shake', type = 'button', bindings = {'key:k', 'button:rightStick'}},
    {name = 'zoom', type = 'axis', positive = {'key:e', 'axis:rightY-'}, negative = {'key:q', 'axis:rightY+'}},
}}

local function raster(path)
    local texture, failure = assets.vectorImage(path):rasterize(2):await()
    if not texture then
        error(failure, 0)
    end
    return texture
end

function Playroom:load(context)
    context:progress(0, 'Drawing the toys')
    self.toys = raster('toys/toys.svg')
    context:progress(0.5, 'Building the bin')
    self.wallpaper = raster('room/wallpaper.svg')
    self.plank = raster('room/plank.svg')
    self.chute = assets.vectorImage('room/chute.svg')
end

function Playroom:enter()
    input.loadActions(Playroom.actions)
    self.bin = ToyBin.new(self.toys, m.random(11))
    self.bin:pour(Playroom.start)
    self.planks = graphics2d.newNineSlice(self.plank, {borders = {40, 40, 40, 40}, fill = 'tile'})
    self.room = graphics2d.newParallax(self.wallpaper, {size = {256, 256}, repeatX = true, repeatY = true, scrollScale = {0.5, 0.5}})
    self.camera = graphics2d.newCamera()
    self.camera.maxShakeOffset = 30
    local plank = ToyBin.plank
    self.view = m.rect(-ToyBin.width / 2 - plank - 60, -ToyBin.height - plank - Playroom.chuteSize[2] - 40, ToyBin.width + plank * 2 + 120, ToyBin.height + plank * 2 + Playroom.chuteSize[2] + 100)
    self.camera.limits = self.view
    self.zoomed = false
    self.pinchScale = 1
    self.blasts = {}
    self.sweep = 0
    self.stepTotal, self.steps = 0, 0
    self.hud = Hud.new(self, {
        title = 'Many Bodies',
        caption = 'A toy bin of physics bodies of four shapes, drawn from one sprite batch.',
        rows = {
            {id = 'toys', label = 'Toys'},
            {id = 'awake', label = 'Awake bodies'},
            {id = 'contacts', label = 'Contacts'},
            {id = 'stepTime', label = 'Physics step'},
            {id = 'waiting', label = 'Waiting in the chutes'},
        },
        controls = {
            ui.row{gap = 12,
                ui.button{id = 'add', text = 'Pour ' .. Hud.count(Playroom.step), variant = 'primary', grow = 1, onClick = function() self.bin:pour(Playroom.step) end},
                ui.button{id = 'remove', text = 'Remove ' .. Hud.count(Playroom.step), grow = 1, onClick = function() self.bin:remove(Playroom.step) end},
            },
            ui.row{gap = 12,
                ui.button{id = 'blast', text = 'Blast', grow = 1, onClick = function() self:blastPile() end},
                ui.button{id = 'shake', text = 'Shake', grow = 1, onClick = function() self:shake() end},
                ui.button{id = 'fit', text = 'Fit view', grow = 1, onClick = function() self.zoomed = false end},
            },
        },
        hint = 'Plus and minus or the triggers pour and remove toys, B or the west button sets off a blast in the pile, and K or a click of the right stick shakes the bin. A click or a tap on the bin sets off a blast there, and the wheel, a pinch, Q and E or the right stick zoom.',
        focus = 'add',
    })
    if haylen.platform == 'headless' then
        Tour.start(self, self:tourSteps())
    end
end

-- The steps of the automatic run: pouring, a blast, a shake, removing toys and an empty bin.
function Playroom:tourSteps()
    local function step(change, text)
        return function()
            change()
            return string.format('%s, %s toys with %s waiting', text, Hud.count(self.bin:count()), Hud.count(self.bin.pending))
        end
    end
    return {
        step(function() end, 'pouring'),
        step(function() self.bin:pour(Playroom.step) end, 'pouring more'),
        step(function() end, 'settling'),
        step(function() self:blastPile() end, 'a blast'),
        step(function() self:shake() end, 'a shake'),
        step(function() self:blastAt(0, -200) end, 'a blast at the floor'),
        step(function() self.bin:remove(Playroom.step) end, 'removing'),
        step(function() self.bin:remove(self.bin:count() + self.bin.pending) end, 'an empty bin'),
        step(function() self.bin:pour(Playroom.step) end, 'pouring again'),
    }
end

function Playroom:blastAt(x, y)
    local reached = self.bin:explode(x, y)
    self.blasts[#self.blasts + 1] = {x = x, y = y, age = 0}
    self.camera:addTrauma(math.min(0.7, 0.3 + reached / 4000))
end

-- A blast at a random toy of the pile, for the keyboard, gamepads and remotes.
function Playroom:blastPile()
    local count = self.bin:count()
    if count == 0 then
        self:blastAt(0, -ToyBin.height / 2)
        return
    end
    local body = self.bin.bodies[self.bin.random:integer(1, count)]
    self:blastAt(body.x, body.y)
end

function Playroom:shake()
    self.bin:startShake()
    self.camera:shake(0.6, 1, 0)
end

-- Fits the bin into the play area until the player zooms, and keeps the zoom between that fit and a few times closer.
function Playroom:frameView(stage)
    local camera = self.camera
    camera.viewport = stage
    local fit = math.min(stage.width / self.view.width, stage.height / self.view.height)
    camera.minZoom = fit
    camera.maxZoom = fit * Playroom.maximumZoom
    if not self.zoomed then
        local center = self.view:center()
        camera.zoom = {fit, fit}
        camera:snapTo(center.x, center.y)
    end
end

function Playroom:zoomAt(factor, x, y)
    self.zoomed = true
    self.camera:zoomAt(factor, x, y)
    self.camera:clampToLimits()
end

-- Taps and clicks on the bin set off blasts, the wheel and pinches zoom at the pointer, and the zoom action zooms at the middle of the play area.
function Playroom:readPointer(stage, dt)
    for _, gesture in ipairs(input.gestures()) do
        if gesture.type == 'tap' and stage:contains({gesture.x, gesture.y}) and not ui.usingPointer() then
            self:blastAt(self.camera:screenToWorld(gesture.x, gesture.y))
        elseif gesture.type == 'pinch' then
            self:zoomAt(gesture.scale / self.pinchScale, gesture.x, gesture.y)
            self.pinchScale = gesture.scale
        end
    end
    if #input.touches() < 2 then
        self.pinchScale = 1
    end
    local zoom = input.value('zoom')
    if zoom ~= 0 then
        local center = stage:center()
        self:zoomAt(math.exp(zoom * Playroom.zoomSpeed * dt), center.x, center.y)
    end
end

function Playroom:event(event)
    if event.type == 'mouseScroll' and self.stage and not ui.usingPointer() then
        local x, y = input.mousePosition()
        if self.stage:contains({x, y}) then
            self:zoomAt(event.scrollY > 0 and 1.15 or 1 / 1.15, x, y)
        end
    end
end

function Playroom:fixedUpdate(step)
    self.bin:spawnRows()
    self.bin:step(step)
    self.stepTotal = self.stepTotal + self.bin.world:stats().stepMilliseconds
    self.steps = self.steps + 1
end

function Playroom:update(dt)
    local stage = self.hud:stage()
    if not stage then
        return
    end
    self.stage = stage
    self:frameView(stage)
    self:readPointer(stage, dt)
    if input.pressed('more') then
        self.bin:pour(Playroom.step)
    end
    if input.pressed('fewer') then
        self.bin:remove(Playroom.step)
    end
    if input.pressed('blast') then
        self:blastPile()
    end
    if input.pressed('shake') then
        self:shake()
    end

    self.bin:sync()
    self.sweep = self.sweep + dt
    if self.sweep >= Playroom.sweepInterval then
        self.sweep = 0
        self.bin:removeEscaped()
    end
    for index = #self.blasts, 1, -1 do
        local blast = self.blasts[index]
        blast.age = blast.age + dt
        if blast.age >= Playroom.blastTime then
            table.remove(self.blasts, index)
        end
    end
    self.camera:update(dt)

    if self.hud:update(dt) then
        local stats = self.bin.world:stats()
        self.hud:show('toys', Hud.count(self.bin:count()))
        self.hud:show('awake', Hud.count(stats.awakeBodies))
        self.hud:show('contacts', Hud.count(stats.contacts))
        self.hud:show('stepTime', Hud.milliseconds(self.steps > 0 and self.stepTotal / self.steps or 0))
        self.hud:show('waiting', Hud.count(self.bin.pending))
        self.stepTotal, self.steps = 0, 0
    end
end

function Playroom:render()
    if not self.stage then
        return
    end
    graphics2d.beginWorld(self.camera)
    self.room:draw(self.camera, {layer = 0})
    local x, y = self.bin.bin:renderTransform()
    graphics2d.drawRect({x - ToyBin.width / 2, y - ToyBin.height, ToyBin.width, ToyBin.height}, Playroom.insideColor, {layer = 1})
    self.bin:draw(self.planks)

    local chuteWidth, chuteHeight = Playroom.chuteSize[1], Playroom.chuteSize[2]
    for _, chute in ipairs(ToyBin.chutes) do
        graphics2d.drawVector(self.chute, chute + x, y - ToyBin.height - ToyBin.plank + 12, {width = chuteWidth, height = chuteHeight, pivotX = 0.5, pivotY = 1, layer = 4})
    end
    for _, blast in ipairs(self.blasts) do
        local progress = blast.age / Playroom.blastTime
        local radius = ToyBin.blast.radius * (0.25 + 0.75 * progress)
        graphics2d.drawCircle(blast.x, blast.y, radius * 0.6, m.color(1, 0.8, 0.4, 0.8 * (1 - progress)), {layer = 5, blend = 'additive'})
        graphics2d.drawRing(blast.x, blast.y, radius, 24 * (1 - progress) + 4, m.color(1, 0.95, 0.8, 0.9 * (1 - progress)), {layer = 5, blend = 'additive'})
    end
end

return Playroom
