-- MessagePack: a match state encoded to bytes and decoded on the frame thread, a large message decoded off the frame thread while the frame keeps running, and messages that are refused.
local datetime = require('datetime')
local haylen = require('haylen')
local net = require('haylen.net')
local ui = require('haylen.ui')

local Test = require('harness.test')

local MessagePack = haylen.class('MessagePack', Test)

MessagePack.code = [[
local bytes = net.encodeMessagePack({tick = 812, players = {{id = 1, x = 40.5, hp = 3}}})
local state = net.decodeMessagePack(bytes)
local replay = net.decodeMessagePackAsync(large):await()  -- Decodes on the task pool.
local ok, message = pcall(net.decodeMessagePack, '\193')  -- Broken bytes raise.]]

MessagePack.kept = 24

function MessagePack:enter()
    self.results = {}
    self:frame{
        code = MessagePack.code,
        hint = 'Encode a match state, decode a large message off the frame thread, or feed broken bytes.',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 520, align = 'stretch', gap = 12,
                ui.button{id = 'small', text = 'Encode a state', variant = 'primary', align = 'stretch', onClick = function() self:roundTrip() end},
                ui.button{id = 'large', text = 'Decode 20000 frames', align = 'stretch', onClick = function() self:decodeLarge() end},
                ui.button{id = 'broken', text = 'Decode broken bytes', align = 'stretch', onClick = function() self:decodeBroken() end},
                ui.label{text = 'The large message decodes on the task pool while the frame keeps running, and broken bytes raise an error that says where they broke.', color = 'textMuted', font = 'caption'},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Results'},
                ui.scroll{id = 'scroll', grow = 1, height = 0, ui.column{id = 'results', gap = 12}},
            },
        }},
    }
    self:roundTrip()
    self:decodeLarge()
    self:decodeBroken()
end

-- Adds a result at the end, the newest at the bottom, with a badge of what happened.
function MessagePack:note(badge, tone, line)
    self.results[#self.results + 1] = {badge = badge, tone = tone, line = line}
    if #self.results > MessagePack.kept then
        table.remove(self.results, 1)
    end
    local nodes = {}
    for index, result in ipairs(self.results) do
        nodes[index] = ui.card{id = 'result-' .. index, align = 'stretch', padding = {12, 18},
            ui.row{gap = 16, ui.badge{text = result.badge, tone = result.tone, width = 150}, ui.label{text = result.line, grow = 1}},
        }
    end
    self.gui:replaceChildren('results', nodes)
    self.gui:command('scroll', 'scrollTo', {node = 'result-' .. #nodes, align = 'end'})
end

function MessagePack:roundTrip()
    local bytes = net.encodeMessagePack({tick = 812, players = {{id = 1, x = 40.5, hp = 3}, {id = 2, x = -12, hp = 0}}, over = false})
    local state = net.decodeMessagePack(bytes)
    self:note('Round trip', 'success', string.format('Encoded the state in %d bytes and read tick %d with %d players, the first at x %.1f.', #bytes, state.tick, #state.players, state.players[1].x))
end

function MessagePack:decodeLarge()
    local frames = {}
    for index = 1, 20000 do
        frames[index] = {tick = index, inputs = {index % 4, index % 7}}
    end
    local bytes = net.encodeMessagePack({frames = frames})
    local started = datetime.now():millis()
    self:spawn(function()
        local value = net.decodeMessagePackAsync(bytes):await()
        self:note('Off the frame', 'information', string.format('Decoded %d bytes into %d frames on the task pool in %d ms.', #bytes, #value.frames, datetime.now():millis() - started))
    end)
end

function MessagePack:decodeBroken()
    for _, bytes in ipairs({'\147\1', '\7\7', '\212\1\2', '\193'}) do
        local ok, message = pcall(net.decodeMessagePack, bytes)
        if ok then
            self:note('Accepted', 'danger', 'The broken bytes decoded, which is a failure.')
        else
            self:note('Refused', 'warning', (message:gsub('^.-:%d+: ', '')))
        end
    end
end

return MessagePack
