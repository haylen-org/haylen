-- The trip in progress: the radar while the app looks for a driver, the car of the driver coming to the pickup and then riding the route, under a sheet with the stage, the minutes left, the driver, the chat, the safety tools and the cancel, and the rating with a tip once the ride arrives.
local assets = require('haylen.assets')
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local MapView = require('map-view')
local Page = require('page')
local Places = require('data.places')
local Rides = require('rides')
local Shell = require('shell')

local Trip = haylen.class('Trip', Page)

Trip.stars = {{id = '1', text = '1'}, {id = '2', text = '2'}, {id = '3', text = '3'}, {id = '4', text = '4'}, {id = '5', text = '5'}}
Trip.tips = {{id = '0', text = 'No tip'}, {id = '1', text = '$1'}, {id = '2', text = '$2'}, {id = '5', text = '$5'}}
Trip.replies = {'I am at the pickup point.', 'I will be there in two minutes.', 'I am wearing a red jacket.'}
Trip.reasons = {{id = 'plans', text = 'My plans changed'}, {id = 'long', text = 'The driver takes too long'}, {id = 'wrong', text = 'I picked the wrong place'}, {id = 'other', text = 'Another reason'}}

function Trip:enter()
    self.trip = Rides.current
    self.map = MapView()
    self.font = assets.font('fonts/barlow-semibold.ttf')
    self.stars, self.tip = 5, 0
    self.chat = {}
    local trip = self.trip
    local driver = trip.driver
    local replies = {gap = 8, wrap = true}
    for index, reply in ipairs(Trip.replies) do
        replies[index] = ui.button{id = 'reply-' .. index, text = reply, onClick = function()
            self:sendChat(reply)
        end}
    end
    self:mountGui(ui.stack{onCancel = function()
        Shell.back()
    end,
        ui.row{align = 'start', padding = 12, gap = 10,
            ui.card{padding = 4, ui.button{id = 'back', icon = Shell.icon('back'), variant = 'icon', accessibilityLabel = 'Back to the map', tooltip = 'Back', onClick = Shell.back}},
            ui.card{padding = 4, ui.button{id = 'recenter', icon = Shell.icon('locate'), variant = 'icon', accessibilityLabel = 'Show the whole route', tooltip = 'Show the whole route', onClick = function()
                self.map:reset()
            end}},
        },
        ui.card{id = 'sheet', anchor = 'stretchBottom', gap = 12, padding = {18, 20, 20, 20}, margin = {0, 12, 12, 12},
            ui.row{gap = 12,
                ui.column{gap = 2, grow = 1,
                    ui.label{id = 'stage', text = '', font = 'heading'},
                    ui.label{id = 'message', text = '', color = 'textMuted'},
                },
                ui.column{id = 'minutesBox', gap = 0, ui.label{id = 'minutes', text = '', font = 'title', color = 'accentText', align = 'end'}, ui.label{text = 'min', font = 'caption', color = 'textMuted', align = 'end'}},
            },
            ui.busyIndicator{id = 'searching', size = 40, align = 'start'},
            ui.row{id = 'driver', gap = 14, visible = false,
                ui.avatar{name = driver.name, size = 60},
                ui.column{gap = 0, grow = 1,
                    ui.label{text = driver.name, font = 'button', wrap = false},
                    ui.label{text = string.format('%s · %s stars', driver.car, driver.rating), font = 'caption', color = 'textMuted', wrap = false},
                },
                ui.badge{text = driver.plate, tone = 'accent', solid = true},
            },
            ui.row{id = 'actions', gap = 10,
                ui.button{id = 'chat', text = 'Chat', icon = Shell.icon('chat'), iconColor = 'text', grow = 1, onClick = function(event)
                    event.gui:set('chatDialog', {open = true})
                end},
                ui.button{id = 'safety', text = 'Safety', icon = Shell.icon('shield'), iconColor = 'text', grow = 1, onClick = function(event)
                    event.gui:set('safetyDialog', {open = true})
                end},
                ui.button{id = 'cancel', text = 'Cancel', icon = Shell.icon('close'), variant = 'destructive', grow = 1, onClick = function(event)
                    event.gui:set('cancelDialog', {open = true})
                end},
            },
            ui.column{id = 'rating', visible = false, gap = 10,
                ui.label{text = 'How was your ride with ' .. driver.name .. '?', font = 'button'},
                ui.segmentedControl{id = 'stars', items = Trip.stars, selected = '5', onChange = function(event)
                    self.stars = tonumber(event.value)
                end},
                ui.label{text = 'Add a tip', font = 'caption', color = 'textMuted'},
                ui.segmentedControl{id = 'tip', items = Trip.tips, selected = '0', onChange = function(event)
                    self.tip = tonumber(event.value)
                end},
                ui.button{id = 'done', text = 'Done', variant = 'primary', align = 'stretch', onClick = function()
                    self:finish()
                end},
            },
        },
        ui.dialog{id = 'chatDialog', title = 'Chat with ' .. driver.name, message = 'Messages go to your driver.', buttons = {{id = 'close', text = 'Close'}},
            ui.column{id = 'chatLines', gap = 6},
            ui.row(replies),
        },
        ui.dialog{id = 'safetyDialog', title = 'Safety', message = 'Share where you are with the people you trust, or call for help at once.',
            buttons = {{id = 'close', text = 'Close'}, {id = 'share', text = 'Share my trip'}, {id = 'help', text = 'Call for help', variant = 'destructive'}},
            onAnswer = function(event)
                if event.button == 'share' then
                    Shell.notify('Your trip is shared with your trusted contacts.')
                elseif event.button == 'help' then
                    Shell.notify('Calling for help is not part of this sample.', 'warning')
                end
            end},
        ui.dialog{id = 'cancelDialog', title = 'Cancel the ride?', message = 'Tell us why, so we can do better.',
            buttons = {{id = 'keep', text = 'Keep the ride'}, {id = 'cancel', text = 'Cancel the ride', variant = 'destructive'}},
            onAnswer = function(event)
                if event.button == 'cancel' then
                    self:cancel()
                end
            end,
            ui.radioGroup{id = 'reason', items = Trip.reasons, selected = 'plans'},
        },
    })
    events.on('tripChanged', function(changed)
        if changed == trip then
            self:showStage()
        end
    end, {owner = self})
    self:showStage()
end

function Trip:layoutChanged(wide)
    self.gui:set('sheet', wide and {anchor = 'topRight', width = 500, margin = 16} or {anchor = 'stretchBottom', width = 'auto', margin = {0, 12, 12, 12}})
end

function Trip:showStage()
    local trip = self.trip
    local stage = Rides.stages[trip.stage]
    local gui = self.gui
    gui:set('stage', {text = stage.title})
    gui:set('message', {text = stage.message})
    gui:set('searching', {visible = stage.id == 'searching'})
    gui:set('driver', {visible = stage.id ~= 'searching'})
    gui:set('actions', {visible = stage.id ~= 'arrived'})
    gui:set('cancel', {visible = stage.id == 'searching' or stage.id == 'coming' or stage.id == 'waiting'})
    gui:set('rating', {visible = stage.id == 'arrived'})
    gui:set('minutesBox', {visible = stage.id ~= 'arrived'})
end

function Trip:sendChat(text)
    self.chat[#self.chat + 1] = ui.label{text = 'You: ' .. text}
    self.chat[#self.chat + 1] = ui.label{text = self.trip.driver.name .. ': Thanks, see you soon!', color = 'textMuted'}
    self.gui:replaceChildren('chatLines', self.chat)
end

function Trip:cancel()
    Rides.cancel()
    Shell.notify('Your ride is cancelled.', 'information')
    Shell.back()
end

function Trip:finish()
    local name = Places.find(self.trip.to).name
    Rides.finish(self.stars, self.tip)
    Shell.notify('Thanks for riding. Your trip to ' .. name .. ' is in your activity.')
    Shell.back()
end

function Trip:update(dt)
    Page.update(self, dt)
    local trip = self.trip
    if trip.stage < #Rides.stages then
        self.gui:set('minutes', {text = tostring(Rides.minutesLeft(trip))})
    end
    local sheet = self.gui:bounds('sheet')
    if not sheet then
        return
    end
    local visible = viewport.safeRect()
    local area = {x = visible.x, y = visible.y + 80, width = visible.width, height = sheet.y - visible.y - 80}
    if self.wide then
        area = {x = visible.x, y = visible.y + 80, width = sheet.x - visible.x, height = visible.height - 80}
    end
    local points = trip.stage <= 3 and trip.approach or trip.route
    if trip.stage == 1 then
        points = {trip.route[1], trip.route[2]}
    end
    self.map:fit(MapView.bounds(points, 500), area, dt, not self.placed)
    self.placed = true
end

function Trip:render()
    Page.render(self)
    local trip = self.trip
    local map = self.map
    map:begin()
    map:drawTraffic()
    local accent = ui.themeColor('accent')
    local route = trip.route
    if trip.stage == 1 then
        map:drawRadar(route[1], route[2], accent)
        map:drawPulse(route[1], route[2], accent)
        return
    end
    if trip.stage <= 3 then
        map:drawRoute(trip.approach, nil, '#FFFFFFFF')
        map:drawPin(route[1], route[2], map.icons.pin, accent, 'Pickup', self.font)
    else
        map:drawRoute(route, math.min(trip.length, (trip.elapsed - Rides.stageStart(trip, 4)) * Rides.speed), accent)
        map:drawPin(route[#route - 1], route[#route], map.icons.flag, '#FFFFFFFF', Places.find(trip.to).name, self.font)
    end
    local x, y, dx, dy = Rides.carPosition(trip)
    map:drawCar(x, y, dx, dy, accent, 1.5)
end

return Trip
