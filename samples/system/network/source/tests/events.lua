-- Connection events: a socket reports open, message, error, disconnect, reconnecting and close to its own listeners, and the event bus hears websocket_connected, websocket_disconnected and websocket_reconnecting from every socket, together with network_online and network_offline where the platform reports the network. net.openSockets counts the sockets the engine keeps alive.
local haylen = require('haylen')
local net = require('haylen.net')
local ui = require('haylen.ui')

local sample = require('sample')
local services = require('services')

local Events = haylen.class('Events', sample.Test)

Events.hints = 'Open and close sockets and watch both kinds of events arrive. The counters on the left add up every event this visit heard.'
Events.focus = 'echo'

local kSocketEvents = {'open', 'message', 'error', 'disconnect', 'reconnecting', 'close'}
local kBusEvents = {'websocket_connected', 'websocket_disconnected', 'websocket_reconnecting', 'network_online', 'network_offline', 'app_background', 'app_active'}

-- Formats one value of an event, with strings quoted so an empty reason still shows.
local function format(value)
    if type(value) == 'string' then
        return string.format('%q', value)
    end
    if math.type(value) == 'float' then
        return string.format('%.2f', value)
    end
    return tostring(value)
end

-- Describes the values of an event in one line.
local function describe(values)
    local parts = {}
    for index = 1, values.n do
        local value = values[index]
        if type(value) == 'table' then
            local fields = {}
            for key, field in pairs(value) do
                fields[#fields + 1] = key .. ' = ' .. format(field)
            end
            table.sort(fields)
            parts[#parts + 1] = '{' .. table.concat(fields, ', ') .. '}'
        else
            parts[#parts + 1] = format(value)
        end
    end
    return table.concat(parts, ', ')
end

function Events:init(entry)
    Events.super.init(self, entry)
    self.counts = {}
    self.lines = {}
    self.logged = 0
end

function Events:content()
    local counters = {}
    for _, name in ipairs(kSocketEvents) do
        counters[#counters + 1] = {id = 'socket ' .. name, cells = {'socket:on', name, 0}}
    end
    for _, name in ipairs(kBusEvents) do
        counters[#counters + 1] = {id = 'bus ' .. name, cells = {'events.on', name, 0}}
    end
    self.counters = counters
    return {
        ui.panel{width = 760, align = 'stretch', gap = 12,
            ui.grid{columns = 2, gap = 12,
                ui.button{id = 'echo', text = 'Open the echo socket', align = 'stretch', onClick = function()
                    self:open(services.echo, nil)
                end},
                ui.button{id = 'failing', text = 'Open a failing socket', align = 'stretch', onClick = function()
                    self:open(services.unreachable, {initialDelay = 0.5, maxDelay = 2, maxAttempts = 3})
                end},
                ui.button{id = 'hello', text = 'Send hello', align = 'stretch', onClick = function()
                    self:sendHello()
                end},
                ui.button{id = 'closeAll', text = 'Close every socket', align = 'stretch', onClick = function()
                    for _, socket in ipairs(self.sockets) do
                        socket:close()
                    end
                end},
            },
            ui.label{id = 'open', text = '', color = 'accentText'},
            ui.scroll{height = 0, grow = 1, ui.table{id = 'counters', columns = {{text = 'Source', width = 190}, {text = 'Event'}, {text = 'Count', width = 110, align = 'end'}}, rows = counters}},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Timeline'},
            ui.scroll{height = 0, grow = 1, focusable = false, ui.list{id = 'timeline', items = {}}},
        },
    }
end

-- The bus listeners belong to the scene, so they end when the player leaves.
function Events:enter()
    Events.super.enter(self)
    for _, name in ipairs(kBusEvents) do
        self:listen(name, function(...)
            self:record('bus', name, table.pack(...))
        end)
    end
end

function Events:update(dt)
    local open = net.openSockets()
    if open ~= self.openCount then
        self.openCount = open
        self:show('open', {text = string.format('net.openSockets() is %d', open)})
    end
end

function Events:record(source, name, values)
    local key = source .. ' ' .. name
    self.counts[key] = (self.counts[key] or 0) + 1
    for _, row in ipairs(self.counters) do
        if row.id == key then
            row.cells[3] = self.counts[key]
        end
    end
    self:show('counters', {rows = self.counters})
    self.logged = self.logged + 1
    table.insert(self.lines, 1, {id = 'line-' .. self.logged, text = (source == 'bus' and 'events: ' or 'socket: ') .. name, caption = sample.clock() .. '  ' .. describe(values)})
    self.lines[100] = nil
    self:show('timeline', {items = self.lines})
end

-- Opens a socket, with reconnection when settings are given, and records every event it reports.
function Events:open(url, reconnect)
    local socket = self:keep(net.websocket(url, {reconnect = reconnect or false}))
    for _, name in ipairs(kSocketEvents) do
        self:on(socket, name, function(...)
            self:record('socket', name, table.pack(...))
        end)
    end
end

function Events:sendHello()
    for _, socket in ipairs(self.sockets) do
        if socket.state == 'open' then
            socket:send('hello ' .. sample.clock())
        end
    end
end

return Events
