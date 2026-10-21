-- The ride to a destination: the map with the route, the pickup and the destination, and a sheet with the six kinds of ride with their prices and times, the payment and the button that requests the ride.
local assets = require('haylen.assets')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local MapView = require('map-view')
local Page = require('page')
local Places = require('data.places')
local Rides = require('rides')
local Shell = require('shell')
local Style = require('style')

local Preview = haylen.class('Preview', Page)

function Preview:enter()
    self.map = MapView()
    self.font = assets.font('fonts/barlow-semibold.ttf')
    self.from = Places.here
    self.to = Places.find(self.params.to)
    self.route, self.length = Rides.route(self.from, self.to)
    self.kind = 'compact'
    self.payment = Rides.wallet.payment
    self.kinds = {}
    for _, kind in ipairs(Rides.kinds) do
        self.kinds[#self.kinds + 1] = {
            id = kind.id,
            type = 'kind',
            name = kind.name,
            art = kind.art,
            seats = kind.seats .. (kind.seats == 1 and ' seat' or ' seats') .. ' · ' .. kind.note,
            price = Rides.money(Rides.price(kind, self.length)),
            eta = kind.minutes .. ' min away',
        }
    end
    self:mountGui(ui.stack{id = 'root', onCancel = Shell.back})
end

function Preview:layoutChanged(wide)
    local space = Style.space
    self.gui:replaceChildren('root', {
        Shell.mapButton('back', 'back', 'Back', Shell.back, {anchor = wide and 'topRight' or 'topLeft', margin = space.m}),
        Shell.sheet('sheet', {
            ui.row{gap = space.s,
                ui.column{gap = 0, grow = 1,
                    ui.label{text = 'To ' .. self.to.name, font = 'heading', wrap = false},
                    ui.label{text = string.format('%s · about %d minutes', Rides.distanceText(self.length), Rides.minutes(self.length)), color = 'textMuted'},
                },
            },
            ui.collection{id = 'kinds', height = 84 * 4, gap = space.xxs, selection = 'single', selected = {self.kind}, selectionFollowsFocus = true, rememberFocus = true,
                types = {kind = {template = ui.row{gap = space.m, padding = {space.xs, space.s},
                    ui.image{part = 'art', bind = {image = 'art'}, width = 104, height = 60, fit = 'contain'},
                    ui.column{gap = 0, grow = 1,
                        ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
                        ui.label{part = 'seats', bind = {text = 'seats'}, font = 'caption', color = 'textMuted', wrap = false},
                    },
                    ui.column{gap = 0,
                        ui.label{part = 'price', bind = {text = 'price'}, font = 'button', align = 'end'},
                        ui.label{part = 'eta', bind = {text = 'eta'}, font = 'caption', color = 'textMuted', align = 'end'},
                    },
                }, estimatedSize = 84}},
                onSelect = function(event)
                    self:pickKind(event.item)
                end,
            },
            ui.divider{},
            ui.menuButton{id = 'payment', text = self:paymentText(), icon = Shell.icon(self:paymentIcon()), iconColor = 'text', variant = 'toolbar', items = self:paymentItems(), justify = 'start', align = 'stretch', onSelect = function(event)
                self.payment = event.item
                Rides.wallet.payment = event.item
                event.gui:set('payment', {text = self:paymentText(), icon = Shell.icon(self:paymentIcon())})
            end},
            ui.button{id = 'request', text = '', variant = 'primary', align = 'stretch', height = 72, style = {fonts = {button = {size = Style.type.heading * 0.85}}}, onClick = function()
                self:request()
            end},
        }, wide),
    })
    self.gui:collection('kinds'):setItems(self.kinds)
    self:pickKind(self.kind)
end

function Preview:paymentItems()
    local items = {{id = 'wallet', text = 'Wallet, ' .. Rides.money(Rides.wallet.balance), image = Shell.icon('wallet')}}
    for _, card in ipairs(Rides.wallet.cards) do
        items[#items + 1] = {id = card.id, text = card.text, image = Shell.icon(card.icon)}
    end
    items[#items + 1] = {id = 'cash', text = 'Cash', image = Shell.icon('cash')}
    return items
end

function Preview:paymentText()
    for _, item in ipairs(self:paymentItems()) do
        if item.id == self.payment then
            return item.text
        end
    end
end

function Preview:paymentIcon()
    return self.payment == 'cash' and 'cash' or (self.payment == 'wallet' and 'wallet' or 'card')
end

function Preview:pickKind(id)
    self.kind = id
    local kind = Rides.kind(id)
    self.gui:set('request', {text = 'Request ' .. kind.name .. ', ' .. Rides.money(Rides.price(kind, self.length))})
end

function Preview:request()
    Rides.request(self.from, self.to, self.kind, self.payment)
    Shell.replace(require('screens.trip'))
end

-- Fits the route into the part of the screen the sheet leaves.
function Preview:update(dt)
    Page.update(self, dt)
    local sheet = self.gui:bounds('sheet')
    if not sheet then
        return
    end
    local visible = viewport.safeRect()
    local area = {x = visible.x, y = visible.y, width = visible.width, height = sheet.y - visible.y}
    if self.wide then
        area = {x = sheet.x + sheet.width, y = visible.y, width = visible.x + visible.width - sheet.x - sheet.width, height = visible.height}
    end
    self.map:fit(MapView.bounds(self.route, 400), area, dt, not self.placed)
    self.placed = true
end

function Preview:render()
    Page.render(self)
    local map = self.map
    map:begin()
    map:drawTraffic()
    local route = self.route
    map:drawRoute(route)
    map:drawPickup(route[1], route[2], 'Pickup', self.font)
    map:drawDestination(route[#route - 1], route[#route], Rides.minutes(self.length) .. ' min', self.font)
end

return Preview
