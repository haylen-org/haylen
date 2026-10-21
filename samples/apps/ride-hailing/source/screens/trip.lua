-- The trip in progress: the radar while the app looks for a driver, the car of the driver coming to the pickup and then riding the route, under a sheet with the stage, the minutes left in large numbers, the progress of the trip, the driver and the plate, the chat, the safety tools and the cancel, and the rating with a tip once the ride arrives.
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
local Style = require('style')

local Trip = haylen.class('Trip', Page)

Trip.stars = {{id = '1', text = '1'}, {id = '2', text = '2'}, {id = '3', text = '3'}, {id = '4', text = '4'}, {id = '5', text = '5'}}
Trip.tips = {{id = '0', text = 'No tip'}, {id = '1', text = '$1'}, {id = '2', text = '$2'}, {id = '5', text = '$5'}}
Trip.replies = {'I am at the pickup point.', 'I will be there in two minutes.', 'I am wearing a red jacket.'}
Trip.reasons = {{id = 'plans', text = 'My plans changed'}, {id = 'long', text = 'The driver takes too long'}, {id = 'wrong', text = 'I picked the wrong place'}, {id = 'other', text = 'Another reason'}}
Trip.plate = {colors = {accent = Style.color.ink, onAccent = Style.color.yellow}, fonts = {caption = {size = Style.type.label}}}

function Trip:enter()
    self.trip = Rides.current
    self.map = MapView()
    self.font = assets.font('fonts/barlow-semibold.ttf')
    self.stars, self.tip = 5, 0
    self.chat = {}
    self:mountGui(ui.stack{id = 'root', onCancel = Shell.back})
    events.on('tripChanged', function(changed)
        if changed == self.trip then
            self:showStage()
        end
    end, {owner = self})
end

function Trip:layoutChanged(wide)
    local space = Style.space
    local driver = self.trip.driver
    local replies = {gap = space.xs, wrap = true}
    for index, reply in ipairs(Trip.replies) do
        replies[index] = ui.button{id = 'reply-' .. index, text = reply, onClick = function()
            self:sendChat(reply)
        end}
    end
    self.gui:replaceChildren('root', {
        Shell.mapButton('back', 'back', 'Back to the map', Shell.back, {anchor = wide and 'topRight' or 'topLeft', margin = space.m}),
        Shell.mapButton('recenter', 'locate', 'Show the whole route', function()
            self.map:reset()
        end, {anchor = wide and 'bottomRight' or 'topRight', margin = space.m}),
        Shell.sheet('sheet', {
            ui.row{gap = space.m,
                ui.column{gap = space.xxs, grow = 1,
                    ui.label{id = 'stage', text = '', font = 'heading'},
                    ui.label{id = 'message', text = '', color = 'textMuted'},
                },
                ui.column{id = 'minutesBox', gap = 0, alignItems = 'end',
                    ui.label{id = 'minutes', text = '', font = 'title', style = Style.text('title', Style.type.display)},
                    ui.label{text = 'min', font = 'caption', color = 'textMuted'},
                },
            },
            ui.progress{id = 'progress', value = 0, accessibilityLabel = 'Progress of the trip'},
            ui.row{id = 'searching', gap = space.s, ui.busyIndicator{size = 36, color = 'text'}, ui.label{text = 'Asking the drivers near you', color = 'textMuted'}},
            ui.row{id = 'driver', gap = space.m, visible = false,
                ui.avatar{name = driver.name, size = 64},
                ui.column{gap = 0, grow = 1,
                    ui.label{text = driver.name, font = 'button', wrap = false},
                    ui.row{gap = space.xxs, ui.icon{image = Shell.icon('star'), size = 20, color = 'accentStrong'}, ui.label{text = driver.rating .. ' · ' .. driver.car, font = 'caption', color = 'textMuted', wrap = false}},
                },
                ui.badge{text = driver.plate, tone = 'accent', solid = true, style = Trip.plate},
            },
            ui.row{id = 'actions', gap = space.s,
                self:action('chat', 'chat', 'Message', function(event)
                    event.gui:set('chatDialog', {open = true})
                end),
                self:action('safety', 'shield', 'Safety', function(event)
                    event.gui:set('safetyDialog', {open = true})
                end),
                self:action('share', 'share', 'Share', function()
                    Shell.notify('Your trip is shared with your trusted contacts.')
                end),
                self:action('cancel', 'close', 'Cancel', function(event)
                    event.gui:set('cancelDialog', {open = true})
                end),
            },
            ui.column{id = 'rating', visible = false, gap = space.s,
                ui.label{text = 'How was your ride with ' .. driver.name .. '?', font = 'button'},
                ui.segmentedControl{id = 'stars', items = Trip.stars, selected = '5', accessibilityLabel = 'Stars', onChange = function(event)
                    self.stars = tonumber(event.value)
                end},
                ui.label{text = 'Add a tip', font = 'caption', color = 'textMuted', margin = {space.xs, 0, 0, 0}},
                ui.segmentedControl{id = 'tip', items = Trip.tips, selected = '0', accessibilityLabel = 'Tip', onChange = function(event)
                    self.tip = tonumber(event.value)
                end},
                ui.button{id = 'done', text = 'Done', variant = 'primary', align = 'stretch', height = 72, margin = {space.xs, 0, 0, 0}, onClick = function()
                    self:finish()
                end},
            },
        }, wide),
        ui.dialog{id = 'chatDialog', title = 'Message ' .. driver.name, message = 'Messages go to your driver.', buttons = {{id = 'close', text = 'Close'}},
            ui.column{id = 'chatLines', gap = space.xs},
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
    self:showStage()
end

-- A round action of the trip with its name under it.
function Trip:action(id, icon, text, onClick)
    local space = Style.space
    return ui.column{id = id .. 'Action', gap = space.xxs, grow = 1, alignItems = 'center',
        ui.button{id = id, icon = Shell.icon(icon), iconColor = 'text', width = 60, height = 60, accessibilityLabel = text, onClick = onClick,
            style = {colors = {raised = Style.color.mist, border = Style.color.mist}, metrics = {controlRadius = 30, controlPaddingX = 0}}},
        ui.label{text = text, font = 'caption', color = 'textMuted'},
    }
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
    gui:set('cancelAction', {visible = stage.id == 'searching' or stage.id == 'coming' or stage.id == 'waiting'})
    gui:set('shareAction', {visible = stage.id == 'riding'})
    gui:set('rating', {visible = stage.id == 'arrived'})
    gui:set('minutesBox', {visible = stage.id ~= 'arrived'})
    gui:set('progress', {visible = stage.id ~= 'arrived'})
end

function Trip:sendChat(text)
    local space = Style.space
    self.chat[#self.chat + 1] = ui.row{gap = 0, ui.spacer{grow = 1}, ui.card{padding = {space.xs, space.m}, style = {colors = {raised = Style.color.yellow, border = Style.color.yellow}}, ui.label{text = text}}}
    self.chat[#self.chat + 1] = ui.row{gap = 0, ui.card{padding = {space.xs, space.m}, style = {colors = {raised = Style.color.mist, border = Style.color.mist}}, ui.label{text = 'Thanks, see you soon!'}}, ui.spacer{grow = 1}}
    self.gui:replaceChildren('chatLines', self.chat)
end

function Trip:cancel()
    Rides.cancel()
    Shell.notify('Your ride is cancelled.')
    Shell.back()
end

function Trip:finish()
    local name = Places.find(self.trip.to).name
    Rides.finish(self.stars, self.tip)
    Shell.notify('Thanks for riding. Your trip to ' .. name .. ' is in your trips.', 'success')
    Shell.back()
end

function Trip:update(dt)
    Page.update(self, dt)
    local trip = self.trip
    if trip.stage < #Rides.stages then
        local minutes = tostring(Rides.minutesLeft(trip))
        if self.shownMinutes ~= minutes then
            self.shownMinutes = minutes
            self.gui:set('minutes', {text = minutes})
        end
        local value = math.floor(trip.elapsed / Rides.stageStart(trip, #Rides.stages) * 200) / 200
        if self.shownValue ~= value then
            self.shownValue = value
            self.gui:set('progress', {value = value})
        end
    end
    local sheet = self.gui:bounds('sheet')
    if not sheet then
        return
    end
    local visible = viewport.safeRect()
    local area = {x = visible.x, y = visible.y, width = visible.width, height = sheet.y - visible.y}
    if self.wide then
        area = {x = sheet.x + sheet.width, y = visible.y, width = visible.x + visible.width - sheet.x - sheet.width, height = visible.height}
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
    local route = trip.route
    if trip.stage == 1 then
        map:drawRadar(route[1], route[2])
        map:drawPulse(route[1], route[2])
        return
    end
    if trip.stage <= 3 then
        map:drawRoute(trip.approach)
        map:drawPickup(route[1], route[2], 'Pickup', self.font)
    else
        map:drawRoute(route, math.min(trip.length, (trip.elapsed - Rides.stageStart(trip, 4)) * Rides.speed))
        map:drawDestination(route[#route - 1], route[#route], Places.find(trip.to).name, self.font)
    end
    local x, y, dx, dy = Rides.carPosition(trip)
    map:drawCar(x, y, dx, dy, Style.color.yellow, 1.4)
end

return Trip
