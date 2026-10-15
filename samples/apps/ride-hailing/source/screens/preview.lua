-- The route to a destination: the map with the route, the pickup and the destination, the six kinds of ride with their prices and times, the payment and the button that requests the ride.
local assets = require('haylen.assets')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local MapView = require('map-view')
local Page = require('page')
local Places = require('data.places')
local Rides = require('rides')
local Shell = require('shell')

local Preview = haylen.class('Preview', Page)

function Preview:enter()
    self.map = MapView()
    self.font = assets.font('fonts/barlow-semibold.ttf')
    self.from = Places.here
    self.to = Places.find(self.params.to)
    self.route, self.length = Rides.route(self.from, self.to)
    self.kind = 'compact'
    self.payment = Rides.wallet.payment
    local kinds = {}
    for _, kind in ipairs(Rides.kinds) do
        kinds[#kinds + 1] = {
            id = kind.id,
            type = 'kind',
            name = kind.name,
            art = kind.art,
            seats = kind.seats .. (kind.seats == 1 and ' seat' or ' seats') .. ' · ' .. kind.note,
            price = Rides.money(Rides.price(kind, self.length)),
            eta = kind.minutes .. ' min away',
        }
    end
    self:mountGui(ui.stack{onCancel = Shell.back,
        ui.row{align = 'start', padding = 12, gap = 10,
            ui.card{padding = 4, ui.button{id = 'back', icon = Shell.icon('back'), variant = 'icon', accessibilityLabel = 'Back', tooltip = 'Back', onClick = Shell.back}},
            ui.card{padding = {4, 18, 4, 18}, ui.column{gap = 0,
                ui.label{text = 'To ' .. self.to.name, font = 'button', wrap = false},
                ui.label{text = string.format('%s · %d min', Rides.distanceText(self.length), Rides.minutes(self.length)), font = 'caption', color = 'textMuted'},
            }},
        },
        ui.card{id = 'sheet', anchor = 'stretchBottom', gap = 10, padding = {18, 18, 18, 18}, margin = {0, 12, 12, 12},
            ui.label{text = 'Choose a ride', font = 'heading'},
            ui.collection{id = 'kinds', height = 280, gap = 4, selection = 'single', selected = {'compact'}, selectionFollowsFocus = true, rememberFocus = true,
                types = {kind = {template = ui.row{gap = 14, padding = {6, 10},
                    ui.image{part = 'art', bind = {image = 'art'}, width = 92, height = 54, fit = 'contain'},
                    ui.column{gap = 0, grow = 1,
                        ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
                        ui.label{part = 'seats', bind = {text = 'seats'}, font = 'caption', color = 'textMuted', wrap = false},
                    },
                    ui.column{gap = 0,
                        ui.label{part = 'price', bind = {text = 'price'}, font = 'button', align = 'end'},
                        ui.label{part = 'eta', bind = {text = 'eta'}, font = 'caption', color = 'textMuted', align = 'end'},
                    },
                }, estimatedSize = 70}},
                onSelect = function(event)
                    self:pickKind(event.item)
                end,
            },
            ui.row{gap = 10,
                ui.menuButton{id = 'payment', text = self:paymentText(), icon = Shell.icon('wallet'), variant = 'toolbar', items = self:paymentItems(), grow = 1, justify = 'start', onSelect = function(event)
                    self.payment = event.item
                    Rides.wallet.payment = event.item
                    event.gui:set('payment', {text = self:paymentText()})
                end},
                ui.button{id = 'request', text = '', variant = 'primary', grow = 1, onClick = function()
                    self:request()
                end},
            },
        },
    })
    self.gui:collection('kinds'):setItems(kinds)
    self:pickKind('compact')
end

function Preview:paymentItems()
    local items = {{id = 'wallet', text = 'Wallet, ' .. Rides.money(Rides.wallet.balance)}}
    for _, card in ipairs(Rides.wallet.cards) do
        items[#items + 1] = {id = card.id, text = card.text}
    end
    items[#items + 1] = {id = 'cash', text = 'Cash'}
    return items
end

function Preview:paymentText()
    for _, item in ipairs(self:paymentItems()) do
        if item.id == self.payment then
            return item.text
        end
    end
end

function Preview:pickKind(id)
    self.kind = id
    self.gui:set('request', {text = 'Request ' .. Rides.kind(id).name})
end

function Preview:layoutChanged(wide)
    self.gui:set('sheet', wide and {anchor = 'topRight', width = 520, margin = 16} or {anchor = 'stretchBottom', width = 'auto', margin = {0, 12, 12, 12}})
end

function Preview:request()
    Rides.request(self.from, self.to, self.kind, self.payment)
    Shell.replace(require('screens.trip'))
end

function Preview:update(dt)
    Page.update(self, dt)
    local sheet = self.gui:bounds('sheet')
    if not sheet then
        return
    end
    local visible = viewport.safeRect()
    local area = {x = visible.x, y = visible.y + 80, width = visible.width, height = sheet.y - visible.y - 80}
    if self.wide then
        area = {x = visible.x, y = visible.y + 80, width = sheet.x - visible.x, height = visible.height - 80}
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
    local accent = ui.themeColor('accent')
    map:drawRoute(route, nil, accent)
    map:drawPin(route[1], route[2], map.icons.pin, accent, 'Pickup', self.font)
    map:drawPin(route[#route - 1], route[#route], map.icons.flag, '#FFFFFFFF', Rides.minutes(self.length) .. ' min', self.font)
end

return Preview
