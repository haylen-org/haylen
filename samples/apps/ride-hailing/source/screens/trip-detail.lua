-- The details of a past trip: its route on a map drawn into a render target that the UI shows as a picture, the places, the driver and the receipt.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local MapView = require('map-view')
local Page = require('page')
local Places = require('data.places')
local Rides = require('rides')
local Shell = require('shell')
local Style = require('style')

local TripDetail = haylen.class('TripDetail', Page)

TripDetail.mapSize = {960, 480}

function TripDetail:enter()
    local space = Style.space
    local trip
    for _, past in ipairs(Rides.past) do
        if past.id == self.params.trip then
            trip = past
        end
    end
    self.trip = trip
    local from, to = Places.find(trip.from), Places.find(trip.to)
    self.route = Rides.route(from, to)
    self.target = graphics.newRenderTarget(TripDetail.mapSize[1], TripDetail.mapSize[2], {filter = 'linear'})
    self.mapName = '@route-' .. trip.id
    ui.setImage(self.mapName, self.target)
    self.map = MapView()
    local bounds = MapView.bounds(self.route, 600)
    local zoom = math.min((TripDetail.mapSize[1] - 160) / bounds.width, (TripDetail.mapSize[2] - 160) / bounds.height)
    self.map.camera.zoom = {zoom, zoom}
    self.map.camera.position = {TripDetail.center(bounds.x + bounds.width / 2, TripDetail.mapSize[1] / zoom / 2, Rides.city.width), TripDetail.center(bounds.y + bounds.height / 2, TripDetail.mapSize[2] / zoom / 2, Rides.city.height)}
    local kind = Rides.kind(trip.kind)
    local service = trip.cancelled and 0 or trip.fare * 0.08
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Trip to ' .. to.name),
        ui.scroll{grow = 1, height = 0,
            Shell.centered(Style.size.reading, ui.column{gap = space.l, padding = {space.l, space.l, space.xl, space.l},
                ui.image{image = self.mapName, align = 'stretch', aspectRatio = TripDetail.mapSize[1] / TripDetail.mapSize[2], radius = Style.radius.sheet, fit = 'cover', accessibilityLabel = 'Map of the route'},
                ui.row{gap = space.m, align = 'stretch',
                    ui.column{gap = 0, width = 28, padding = {14, 0}, align = 'stretch',
                        ui.icon{image = Shell.icon('dot'), size = 28, color = 'text', align = 'center'},
                        ui.divider{vertical = true, grow = 1, width = 4, align = 'center', color = 'borderStrong', style = {metrics = {borderWidth = 2.5}}},
                        ui.icon{image = Shell.icon('square'), size = 28, color = 'text', align = 'center'},
                    },
                    ui.column{gap = space.m, grow = 1,
                        ui.column{gap = 0, ui.label{text = from.name, font = 'button'}, ui.label{text = from.line, font = 'caption', color = 'textMuted'}},
                        ui.column{gap = 0, ui.label{text = to.name, font = 'button'}, ui.label{text = to.line, font = 'caption', color = 'textMuted'}},
                    },
                },
                ui.label{text = trip.caption, color = 'textMuted'},
                ui.divider{},
                ui.row{gap = space.m,
                    ui.avatar{name = trip.driver.name, size = 64},
                    ui.column{gap = 0, grow = 1, ui.label{text = trip.driver.name, font = 'button'}, ui.label{text = trip.driver.car .. ' · ' .. trip.driver.plate, font = 'caption', color = 'textMuted'}},
                    ui.image{image = kind.art, width = 112, height = 64, fit = 'contain'},
                },
                ui.card{gap = space.s, padding = space.l, style = {colors = {raised = Style.color.mist, border = Style.color.mist}, metrics = {panelRadius = Style.radius.control}},
                    ui.label{text = 'Receipt', font = 'heading'},
                    self:costRow('Fare', trip.cancelled and 'Cancelled' or Rides.money(trip.fare - service)),
                    self:costRow('Service', Rides.money(service)),
                    ui.divider{},
                    self:costRow('Total', trip.price, 'heading'),
                },
                ui.row{wrap = true, gap = space.s,
                    ui.button{id = 'help', text = 'Get help', icon = Shell.icon('info'), iconColor = 'text', grow = 1, onClick = function()
                        Shell.notify('Our team answers questions about this trip in the chat.')
                    end},
                    ui.button{id = 'again', text = 'Ride there again', variant = 'primary', grow = 1, onClick = function()
                        Shell.open(require('screens.preview'), {to = to.id})
                    end},
                },
            }),
        },
    })
end

-- The center of the view along one axis: around the route, kept inside the city so its edge never shows beside a route near it, or in the middle of the city when the view is wider than the city.
function TripDetail.center(route, half, size)
    if half * 2 >= size then
        return size / 2
    end
    return math.max(half, math.min(size - half, route))
end

function TripDetail:costRow(text, value, font)
    return ui.row{gap = Style.space.s, ui.label{text = text, font = font or 'body', color = font and 'text' or 'textMuted', grow = 1}, ui.label{text = value, font = font or 'body'}}
end

function TripDetail:exit()
    ui.removeImage(self.mapName)
end

-- Draws the route into its render target, which the picture of the page shows.
function TripDetail:render()
    Page.render(self)
    local map = self.map
    graphics2d.beginTarget(self.target, map.camera, {clear = Style.map.ground})
    map.unit = 1.6 / map.camera.zoom.x
    Rides.city:draw(Style.map, map.camera:visibleBounds())
    local route = self.route
    map:drawRoute(route)
    map:drawPickup(route[1], route[2])
    map:drawDestination(route[#route - 1], route[#route])
end

return TripDetail
