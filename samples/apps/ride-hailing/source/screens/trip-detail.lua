-- The details of a past trip: its route on a map drawn into a render target that the UI shows as a picture, the places, the driver and the receipt.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local City = require('city')
local MapView = require('map-view')
local Page = require('page')
local Places = require('data.places')
local Rides = require('rides')
local Shell = require('shell')

local TripDetail = haylen.class('TripDetail', Page)

TripDetail.mapSize = {960, 480}

function TripDetail:enter()
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
    self.camera = graphics2d.newCamera()
    local bounds = MapView.bounds(self.route, 300)
    self.camera.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}
    local zoom = math.min((TripDetail.mapSize[1] - 120) / bounds.width, (TripDetail.mapSize[2] - 120) / bounds.height)
    self.camera.zoom = {zoom, zoom}
    local kind = Rides.kind(trip.kind)
    local service = trip.cancelled and 0 or trip.fare * 0.08
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Trip to ' .. to.name),
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 860, align = 'center', gap = 14, padding = {4, 20, 28, 20},
                ui.image{image = self.mapName, align = 'stretch', aspectRatio = TripDetail.mapSize[1] / TripDetail.mapSize[2], radius = 22, fit = 'cover', accessibilityLabel = 'Map of the route'},
                ui.card{gap = 8,
                    ui.row{gap = 12, ui.icon{image = Shell.icon('pin'), size = 24, color = 'accentText'}, ui.column{gap = 0, grow = 1, ui.label{text = from.name, font = 'button'}, ui.label{text = from.line, font = 'caption', color = 'textMuted'}}},
                    ui.row{gap = 12, ui.icon{image = Shell.icon('flag'), size = 24, color = 'text'}, ui.column{gap = 0, grow = 1, ui.label{text = to.name, font = 'button'}, ui.label{text = to.line, font = 'caption', color = 'textMuted'}}},
                    ui.divider{},
                    ui.label{text = trip.caption, font = 'caption', color = 'textMuted'},
                },
                ui.card{
                    ui.row{gap = 14,
                        ui.avatar{name = trip.driver.name, size = 56},
                        ui.column{gap = 0, grow = 1, ui.label{text = trip.driver.name, font = 'button'}, ui.label{text = trip.driver.car .. ' · ' .. trip.driver.plate, font = 'caption', color = 'textMuted'}},
                        ui.image{image = kind.art, width = 88, height = 52, fit = 'contain'},
                    },
                },
                ui.card{gap = 8,
                    ui.label{text = 'Receipt', font = 'button'},
                    self:costRow('Fare', trip.cancelled and 'Cancelled' or Rides.money(trip.fare - service)),
                    self:costRow('Service', Rides.money(service)),
                    ui.divider{},
                    self:costRow('Total', trip.price, 'heading'),
                },
                ui.row{gap = 12,
                    ui.button{id = 'help', text = 'Get help', icon = Shell.icon('info'), iconColor = 'text', grow = 1, onClick = function()
                        Shell.notify('Our team answers questions about this trip in the chat.', 'information')
                    end},
                    ui.button{id = 'again', text = 'Ride there again', variant = 'primary', grow = 1, onClick = function()
                        Shell.open(require('screens.preview'), {to = to.id})
                    end},
                },
            },
        },
    })
end

function TripDetail:costRow(text, value, font)
    return ui.row{gap = 8, ui.label{text = text, font = font or 'body', color = font and 'text' or 'textMuted', grow = 1}, ui.label{text = value, font = font or 'body'}}
end

function TripDetail:exit()
    ui.removeImage(self.mapName)
end

-- Draws the route into its render target, which the picture of the page shows.
function TripDetail:render()
    Page.render(self)
    local day = ui.theme() == 'rideDay'
    local palette = day and City.light or City.dark
    local unit = 1 / self.camera.zoom.x
    graphics2d.beginTarget(self.target, self.camera, {clear = palette.ground})
    Rides.city:draw(palette, self.camera:visibleBounds())
    local route = self.route
    local accent = ui.themeColor('accent')
    graphics2d.drawStroke(route, {width = 18 * unit, color = '#FF1E1B33', join = 'round', cap = 'round'})
    graphics2d.drawStroke(route, {width = 11 * unit, color = accent, join = 'round', cap = 'round'})
    graphics2d.drawCircle(route[1], route[2], 16 * unit, '#FF1E1B33')
    graphics2d.drawCircle(route[1], route[2], 10 * unit, accent)
    graphics2d.drawCircle(route[#route - 1], route[#route], 16 * unit, '#FF1E1B33')
    graphics2d.drawCircle(route[#route - 1], route[#route], 10 * unit, '#FFFFFFFF')
end

return TripDetail
