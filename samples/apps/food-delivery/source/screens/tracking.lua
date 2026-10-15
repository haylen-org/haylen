-- The tracker of an order: the city map drawn by the engine with the route from the restaurant to the door and the courier riding it, under a sheet with the stage, the time left, the courier and the order. Phones show the sheet at the bottom, and wide windows on the side.
local assets = require('haylen.assets')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local math2d = require('haylen.math')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Catalog = require('data.catalog')
local City = require('city')
local Orders = require('orders')
local Page = require('page')
local Shell = require('shell')

local Tracking = haylen.class('Tracking', Page)

Tracking.steps = {'Confirmed', 'Cooking', 'On the way', 'Delivered'}
-- Room in design units that the route keeps around it on the map.
Tracking.margin = 70
Tracking.minZoom = 0.25
Tracking.maxZoom = 3

function Tracking:enter()
    self.order = Orders.find(self.params.order)
    self.restaurant = Catalog.restaurant(self.order.restaurant)
    self.camera = graphics2d.newCamera()
    self.zoomFactor = 1
    self.pan = {0, 0}
    self.icons = {home = assets.vectorImage('icons/home.svg'), shop = assets.vectorImage('icons/bag.svg'), courier = assets.vectorImage('icons/scooter.svg')}
    local steps = {gap = 6}
    for index, text in ipairs(Tracking.steps) do
        steps[#steps + 1] = ui.column{gap = 4, grow = 1, ui.progress{id = 'step' .. index, value = 0}, ui.label{text = text, font = 'caption', color = 'textMuted', wrap = false}}
    end
    local order = self.order
    self:mountGui(ui.stack{onCancel = Shell.back,
        ui.row{align = 'start', padding = 12, gap = 10,
            ui.card{padding = 4, ui.button{id = 'back', icon = Shell.icon('back'), variant = 'icon', accessibilityLabel = 'Back', tooltip = 'Back', onClick = Shell.back}},
            ui.card{padding = 4, ui.button{id = 'recenter', icon = Shell.icon('locate'), variant = 'icon', accessibilityLabel = 'Show the whole route', tooltip = 'Show the whole route', onClick = function()
                self.zoomFactor = 1
                self.pan = {0, 0}
            end}},
        },
        ui.card{id = 'sheet', anchor = 'stretchBottom', anchorTo = 'screen', gap = 12, padding = {20, 22, 22, 22}, margin = 0, style = {metrics = {panelRadius = 28}},
            ui.row{gap = 12,
                ui.column{gap = 2, grow = 1,
                    ui.label{id = 'stage', text = '', font = 'heading'},
                    ui.label{id = 'message', text = '', color = 'textMuted'},
                },
                ui.column{gap = 0, ui.label{id = 'minutes', text = '', font = 'title', color = 'accentText', textAlign = 'end', align = 'end'}, ui.label{id = 'minutesCaption', text = 'minutes', font = 'caption', color = 'textMuted', align = 'end'}},
            },
            ui.row(steps),
            ui.divider{},
            ui.row{gap = 14,
                ui.avatar{name = order.courier.name, size = 56},
                ui.column{gap = 0, grow = 1,
                    ui.label{text = order.courier.name, font = 'button', wrap = false},
                    ui.label{text = string.format('%s · %s · %s stars', order.courier.vehicle, order.courier.plate, order.courier.rating), font = 'caption', color = 'textMuted'},
                },
                ui.button{id = 'call', icon = Shell.icon('phone'), variant = 'icon', accessibilityLabel = 'Call the courier', tooltip = 'Call', onClick = function()
                    Shell.notify('Calling is not part of this sample.', 'information')
                end},
                ui.button{id = 'chat', icon = Shell.icon('chat'), variant = 'icon', accessibilityLabel = 'Message the courier', tooltip = 'Message', onClick = function()
                    Shell.notify(order.courier.name .. ' read your message.', 'information')
                end},
            },
            ui.accordion{id = 'details', items = {{id = 'order', text = string.format('%s · %d %s · %s', order.name, order.count, order.count == 1 and 'item' or 'items', Catalog.money(order.totals.total))}},
                ui.column{gap = 6, children = self:detailRows()},
            },
        },
    })
    events.on('orderChanged', function(changed)
        if changed == order then
            self:showStage()
        end
    end, {owner = self})
    self:showStage()
end

function Tracking:detailRows()
    local rows = {}
    for _, line in ipairs(self.order.lines) do
        rows[#rows + 1] = ui.row{gap = 8, ui.label{text = line.quantity .. ' × ' .. line.name, grow = 1, font = 'caption'}, ui.label{text = line.total, font = 'caption'}}
    end
    rows[#rows + 1] = ui.label{text = 'To ' .. self.order.address.line, font = 'caption', color = 'textMuted'}
    return rows
end

-- The sheet sits at the bottom of a phone and on the right of a wide window, so the map keeps the rest.
function Tracking:layoutChanged(wide)
    self.gui:set('sheet', wide and {anchor = 'topRight', anchorTo = 'safe', width = 480, margin = 16} or {anchor = 'stretchBottom', anchorTo = 'screen', width = 'auto', margin = 0})
    self.inset = nil
end

-- On a phone the sheet reaches the bottom edge of the screen and keeps its content above the home indicator.
function Tracking:fitSheet()
    local inset = 0
    if not self.wide then
        local visible, safe = viewport.visibleRect(), viewport.safeRect()
        local _, unit = ui.scale()
        inset = math.floor(((visible.y + visible.height) - (safe.y + safe.height)) / unit + 0.5)
    end
    if inset ~= self.inset then
        self.inset = inset
        self.gui:set('sheet', {padding = {20, 22, 22 + inset, 22}})
    end
end

function Tracking:showStage()
    local order = self.order
    local stage = Orders.stages[order.stage]
    local gui = self.gui
    gui:set('stage', {text = stage.title})
    gui:set('message', {text = stage.message})
    local done = order.stage == #Orders.stages
    gui:set('minutes', {text = done and 'Here' or tostring(Orders.minutesLeft(order))})
    gui:set('minutesCaption', {visible = not done})
end

function Tracking:update(dt)
    Page.update(self, dt)
    self:fitSheet()
    local order = self.order
    -- Each step bar fills while its stage runs: the confirmation, the kitchen with the pickup, the ride and the door.
    local starts = {0, Orders.stageStart(order, 2), Orders.stageStart(order, 4), Orders.stageStart(order, 5)}
    local ends = {starts[2], starts[3], starts[4], starts[4] + 0.001}
    for index = 1, 4 do
        local value = math.max(0, math.min(1, (order.elapsed - starts[index]) / (ends[index] - starts[index])))
        self.gui:set('step' .. index, {value = value})
    end
    if order.stage < #Orders.stages then
        self.gui:set('minutes', {text = tostring(Orders.minutesLeft(order))})
    end
    self:moveMap(dt)
end

-- Fits the route into the part of the screen the sheet leaves, and lets a drag pan the map, a pinch or the wheel zoom it.
function Tracking:moveMap(dt)
    local visible = viewport.visibleRect()
    local area = {x = visible.x, y = visible.y, width = visible.width, height = visible.height}
    local sheet = self.gui:bounds('sheet')
    if sheet then
        if self.wide then
            area.width = sheet.x - visible.x
        else
            area.height = sheet.y - visible.y
        end
    end
    local route = self.order.route
    local left, top, right, bottom = math.huge, math.huge, -math.huge, -math.huge
    for index = 1, #route, 2 do
        left, right = math.min(left, route[index]), math.max(right, route[index])
        top, bottom = math.min(top, route[index + 1]), math.max(bottom, route[index + 1])
    end
    local fit = math.min((area.width - Tracking.margin * 2) / math.max(right - left, 1), (area.height - Tracking.margin * 2) / math.max(bottom - top, 1))
    self:readPointer()
    local zoom = math.max(Tracking.minZoom, math.min(Tracking.maxZoom, fit * self.zoomFactor))
    self.zoomFactor = zoom / math.max(fit, 0.0001)
    local centerX = (left + right) / 2 + self.pan[1] - (area.x + area.width / 2 - (visible.x + visible.width / 2)) / zoom
    local centerY = (top + bottom) / 2 + self.pan[2] - (area.y + area.height / 2 - (visible.y + visible.height / 2)) / zoom
    local camera = self.camera
    local blend = 1 - math.exp(-dt * 10)
    camera.zoom = {camera.zoom.x + (zoom - camera.zoom.x) * blend, camera.zoom.x + (zoom - camera.zoom.x) * blend}
    camera.position = {camera.x + (centerX - camera.x) * blend, camera.y + (centerY - camera.y) * blend}
end

-- A drag that starts on the map pans it, a pinch or the wheel zooms it, and presses that start on the sheet or a button stay with the interface.
function Tracking:readPointer()
    local zoom = self.camera.zoom.x
    local touches = input.touches()
    if #touches == 1 then
        local touch = touches[1]
        if touch.phase == 'began' then
            self.dragging = not ui.usingPointer()
        elseif self.dragging then
            self.pan[1], self.pan[2] = self.pan[1] - touch.dx / zoom, self.pan[2] - touch.dy / zoom
        end
    elseif #touches == 0 then
        if input.mousePressed('left') then
            self.dragging = not ui.usingPointer()
        elseif input.mouseDown('left') and self.dragging then
            local dx, dy = input.mouseDelta()
            self.pan[1], self.pan[2] = self.pan[1] - dx / zoom, self.pan[2] - dy / zoom
        end
    end
    local _, wheel = input.mouseScroll()
    if wheel ~= 0 and not ui.usingPointer() then
        self.zoomFactor = self.zoomFactor * (wheel > 0 and 1.1 or 1 / 1.1)
    end
    for _, gesture in ipairs(input.gestures()) do
        if gesture.type == 'pinch' then
            self.pinchBase = self.pinchBase or self.zoomFactor
            self.zoomFactor = self.pinchBase * gesture.scale
        end
    end
    if #touches < 2 then
        self.pinchBase = nil
    end
end

function Tracking:render()
    local order = self.order
    local palette = ui.theme() == 'foodDark' and City.dark or City.light
    local camera = self.camera
    local unit = 1 / camera.zoom.x
    graphics2d.beginWorld(camera)
    Orders.city:draw(palette, camera:visibleBounds())
    local accent = ui.themeColor('accent')
    local route = order.route
    graphics2d.drawStroke(route, {width = 16 * unit, color = '#FF1E1B33', join = 'round', cap = 'round'})
    graphics2d.drawStroke(route, {width = 10 * unit, color = '#FFB8A89C', join = 'round', cap = 'round'})
    local rest = City.routeFrom(route, order.ridden)
    if #rest >= 4 then
        graphics2d.drawStroke(rest, {width = 10 * unit, color = accent, join = 'round', cap = 'round'})
    end
    self:drawPin(route[1], route[2], self.icons.shop, '#FF1E1B33', unit)
    self:drawPin(route[#route - 1], route[#route], self.icons.home, accent, unit)
    if order.stage >= 3 then
        local x, y = City.pointAlong(route, order.ridden)
        local pulse = 1 + 0.12 * math.sin(haylen.elapsed() * 5)
        graphics2d.drawCircle(x, y, 34 * unit * pulse, math2d.color(accent.r, accent.g, accent.b, 0.25))
        graphics2d.drawShape({x - 24 * unit, y - 24 * unit, 48 * unit, 48 * unit}, {radius = 24 * unit, color = '#FFFFFFFF', borderWidth = 4 * unit, borderColor = accent})
        graphics2d.drawVector(self.icons.courier, x, y, {width = 28 * unit, height = 28 * unit, color = accent})
    end
end

-- A round pin with an icon, the same size on the screen at every zoom.
function Tracking:drawPin(x, y, icon, color, unit)
    graphics2d.drawShape({x - 22 * unit, y - 22 * unit, 44 * unit, 44 * unit}, {radius = 22 * unit, color = color, borderWidth = 4 * unit, borderColor = '#FFFFFFFF'})
    graphics2d.drawVector(icon, x, y, {width = 24 * unit, height = 24 * unit, color = '#FFFFFFFF'})
end

return Tracking
