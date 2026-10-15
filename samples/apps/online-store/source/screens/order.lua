-- An order: the van on a map of the city drawn into a render target that the page shows as a picture, the stages of the delivery and the products.
local events = require('haylen.events')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local City = require('city')
local Orders = require('orders')
local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')
local Widgets = require('widgets')

local Order = haylen.class('Order', Page)

Order.mapSize = {960, 480}

function Order:enter()
    local order = Orders.find(self.params.order)
    self.order = order
    local stages = {gap = 6}
    for index, stage in ipairs(Orders.stages) do
        stages[#stages + 1] = ui.row{gap = 12,
            ui.icon{id = 'stageIcon' .. index, image = Shell.icon('check'), size = 26, color = 'textMuted'},
            ui.column{gap = 0, grow = 1, ui.label{id = 'stageTitle' .. index, text = stage.title, font = 'button', color = 'textMuted'}, ui.label{text = stage.message, font = 'caption', color = 'textMuted'}},
        }
    end
    local lines = {gap = 10}
    for _, line in ipairs(order.lines) do
        lines[#lines + 1] = ui.row{gap = 12,
            Widgets.picture({width = 64, height = 70, radius = 12, inset = 6, image = line.image, backdrop = 'products/backdrops/sand.svg', tint = line.tint}),
            ui.label{text = line.quantity .. ' of ' .. line.name, grow = 1},
            ui.label{text = line.total, font = 'button'},
        }
    end
    local live = order.route ~= nil
    if live then
        self.target = graphics.newRenderTarget(Order.mapSize[1], Order.mapSize[2], {filter = 'linear'})
        self.mapName = '@delivery-' .. order.id
        ui.setImage(self.mapName, self.target)
        self.camera = graphics2d.newCamera()
        self.camera.zoom = {0.7, 0.7}
    end
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header(order.number),
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 860, align = 'center', gap = 14, padding = {4, 20, 28, 20},
                ui.image{image = self.mapName or 'images/hero.png', visible = live, align = 'stretch', aspectRatio = Order.mapSize[1] / Order.mapSize[2], radius = 22, fit = 'cover', accessibilityLabel = 'Map of the delivery'},
                ui.card{gap = 4, ui.label{id = 'status', text = '', font = 'heading'}, ui.label{text = 'To ' .. Store.me.address.street .. ', ' .. Store.me.address.city, color = 'textMuted'}},
                ui.card(stages),
                ui.sectionTitle{text = 'Products'},
                ui.card(lines),
                ui.card{gap = 8, ui.row{gap = 8, ui.label{text = 'Total', font = 'heading', grow = 1}, ui.label{text = Store.money(order.total), font = 'heading'}}},
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

function Order:showStage()
    local order = self.order
    self.gui:set('status', {text = Orders.stages[order.stage].title})
    for index = 1, #Orders.stages do
        local done = index <= order.stage
        self.gui:set('stageIcon' .. index, {color = done and 'successText' or 'textMuted'})
        self.gui:set('stageTitle' .. index, {color = done and 'text' or 'textMuted'})
    end
end

function Order:exit()
    if self.mapName then
        ui.removeImage(self.mapName)
    end
end

-- Draws the delivery into its render target: the city around the route, the route still to drive and the van.
function Order:render()
    Page.render(self)
    local order = self.order
    if not order.route then
        return
    end
    local route = order.route
    local x, y, dx, dy = City.pointAlong(route, order.driven)
    local camera = self.camera
    camera.position = {x, y}
    local unit = 1 / camera.zoom.x
    local palette = ui.theme() == 'storeDark' and City.dark or City.light
    graphics2d.beginTarget(self.target, camera, {clear = palette.ground})
    Orders.city:draw(palette, camera:visibleBounds())
    local accent = ui.themeColor('accent')
    local rest = City.routeFrom(route, order.driven)
    if #rest >= 4 then
        graphics2d.drawStroke(rest, {width = 18 * unit, color = '#FF1E1B33', join = 'round', cap = 'round'})
        graphics2d.drawStroke(rest, {width = 11 * unit, color = accent, join = 'round', cap = 'round'})
    end
    local homeX, homeY = route[#route - 1], route[#route]
    graphics2d.drawShape({homeX - 20 * unit, homeY - 20 * unit, 40 * unit, 40 * unit}, {radius = 20 * unit, color = accent, borderWidth = 4 * unit, borderColor = '#FFFFFFFF'})
    local angle = (dx ~= 0 or dy ~= 0) and math.atan(dy, dx) or 0
    graphics2d.drawShape({x - 26 * unit, y - 15 * unit, 52 * unit, 30 * unit}, {radius = 8 * unit, rotation = angle, color = '#FFFFFFFF', borderWidth = 4 * unit, borderColor = '#FF1E1B33'})
    graphics2d.drawShape({x - 6 * unit, y - 15 * unit, 12 * unit, 30 * unit}, {rotation = angle, color = accent})
end

return Order
