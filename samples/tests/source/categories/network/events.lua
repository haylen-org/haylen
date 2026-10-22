-- A socket reports `open`, `message`, `error`, `disconnect`, `reconnecting` and `close` to its own listeners, and the event bus hears `webSocketConnected`, `webSocketDisconnected` and `webSocketReconnecting` from every socket, together with `networkOnline` and `networkOffline` where the platform reports the network. The function `net.openSocketCount` counts the sockets the engine keeps alive.
local haylen = require('haylen')
local net = require('haylen.net')
local ui = require('haylen.ui')

local SocketTest = require('categories.network.socket-test')
local readout = require('categories.network.readout')
local services = require('categories.network.services')

local Events = haylen.class('Events', SocketTest)

Events.code = [[
socket:on('reconnecting', function(attempt, delay) print(attempt, delay) end)
events.on('webSocketReconnecting', function(info) print(info.url, info.attempt, info.delay) end, {owner = self})
events.on('networkOffline', function() showBanner() end, {owner = self})
local open = net.openSocketCount()]]

Events.socketEvents = {'open', 'message', 'error', 'disconnect', 'reconnecting', 'close'}
Events.busEvents = {'webSocketConnected', 'webSocketDisconnected', 'webSocketReconnecting', 'networkOnline', 'networkOffline', 'appBackground', 'appActive'}
-- The failing socket, which the test opens with the echo socket, tries a few times and gives up.
Events.failing = {initialDelay = 0.5, maxDelay = 2, maxAttempts = 3}

-- Each event of the timeline as a sentence made from its values, keyed by its source and name.
Events.sentences = {
    ['socket open'] = function()
        return 'The socket connected.'
    end,
    ['socket message'] = function(data, binary)
        return binary and string.format('A binary message of %s arrived.', readout.bytes(#data)) or string.format('The text "%s" arrived.', data)
    end,
    ['socket error'] = function(message)
        return message
    end,
    ['socket disconnect'] = function(code, reason)
        return string.format('The connection ended with the code %d%s.', code, Events.reason(reason))
    end,
    ['socket reconnecting'] = function(attempt, delay)
        return string.format('Attempt %d comes in %.2f seconds.', attempt, delay)
    end,
    ['socket close'] = function(code, reason)
        return string.format('The socket is done, with the code %d%s.', code, Events.reason(reason))
    end,
    ['bus webSocketConnected'] = function(info)
        return string.format('The socket to "%s" connected%s.', info.url, info.protocol ~= '' and string.format(' with the protocol "%s"', info.protocol) or '')
    end,
    ['bus webSocketDisconnected'] = function(info)
        return string.format('The socket to "%s" lost its connection with the code %d%s.', info.url, info.code, Events.reason(info.reason))
    end,
    ['bus webSocketReconnecting'] = function(info)
        return string.format('The socket to "%s" tries attempt %d in %.2f seconds.', info.url, info.attempt, info.delay)
    end,
    ['bus networkOnline'] = function()
        return 'The device has a network.'
    end,
    ['bus networkOffline'] = function()
        return 'The device lost its network.'
    end,
    ['bus appBackground'] = function()
        return 'The app went to the background.'
    end,
    ['bus appActive'] = function()
        return 'The app came back to the foreground.'
    end,
}

-- The reason of a code, which is often empty.
function Events.reason(reason)
    return reason ~= '' and string.format(' and the reason "%s"', reason) or ''
end

function Events:init(entry)
    Events.super.init(self, entry)
    self.counts = {}
    self.lines = {}
    self.logged = 0
end

-- The bus listeners belong to the scene, so they end when the player leaves.
function Events:enter()
    local counters = {}
    for _, name in ipairs(Events.socketEvents) do
        counters[#counters + 1] = {id = 'socket ' .. name, cells = {'socket:on', name, 0}}
    end
    for _, name in ipairs(Events.busEvents) do
        counters[#counters + 1] = {id = 'bus ' .. name, cells = {'events.on', name, 0}}
    end
    self.counters = counters
    self:frame{
        code = Events.code,
        hint = 'Open and close sockets and watch both kinds of events arrive. The counters on the left add up every event this visit heard.',
        focus = 'echo',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 760, align = 'stretch', gap = 12,
                ui.grid{columns = 2, gap = 12,
                    ui.button{id = 'echo', text = 'Open the echo socket', align = 'stretch', onClick = function()
                        self:open(services.echo, nil)
                    end},
                    ui.button{id = 'failing', text = 'Open a failing socket', align = 'stretch', onClick = function()
                        self:open(services.unreachable, Events.failing)
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
                ui.scroll{grow = 1, height = 0, ui.table{id = 'counters', columns = {{text = 'Source', width = 190}, {text = 'Event'}, {text = 'Count', width = 110, align = 'end'}}, rows = counters}},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Timeline'},
                ui.label{id = 'timelineEmpty', text = 'Every event of the sockets and of the bus lands here, the newest first.', color = 'textMuted'},
                ui.scroll{grow = 1, height = 0, focusable = false, ui.list{id = 'timeline', items = {}}},
            },
        }},
    }
    for _, name in ipairs(Events.busEvents) do
        self:listen(name, function(...)
            self:record('bus', name, table.pack(...))
        end)
    end
    self:open(services.echo, nil)
    self:open(services.unreachable, Events.failing)
end

function Events:update(dt)
    Events.super.update(self, dt)
    local open = net.openSocketCount()
    if open ~= self.openCount then
        self.openCount = open
        self:set('open', {text = string.format('The call "net.openSocketCount()" returns %d.', open)})
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
    self:set('counters', {rows = self.counters})
    self.logged = self.logged + 1
    table.insert(self.lines, 1, {id = 'line-' .. self.logged, text = (source == 'bus' and 'Bus event "' or 'Socket event "') .. name .. '"', caption = readout.clock() .. '  ' .. Events.sentences[key](table.unpack(values, 1, values.n))})
    self.lines[100] = nil
    self:set('timeline', {items = self.lines})
    self:set('timelineEmpty', {visible = false})
end

-- Opens a socket, with reconnection when settings are given, and records every event it reports.
function Events:open(url, reconnect)
    local socket = self:keep(net.connectWebSocket(url, {reconnect = reconnect or false}))
    for _, name in ipairs(Events.socketEvents) do
        self:on(socket, name, function(...)
            self:record('socket', name, table.pack(...))
        end)
    end
end

function Events:sendHello()
    for _, socket in ipairs(self.sockets) do
        if socket.state == 'open' then
            socket:send('Hello at ' .. readout.clock())
        end
    end
end

return Events
