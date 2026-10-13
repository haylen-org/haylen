-- MessagePack: a match state encoded to bytes and decoded on the frame thread, a large message decoded off the frame thread while the frame keeps running, and messages that are refused.
local datetime = require('datetime')
local haylen = require('haylen')
local net = require('haylen.net')
local ui = require('haylen.ui')

local Test = require('harness.test')

local MessagePack = haylen.class('MessagePack', Test)

function MessagePack:enter()
    self.lines = {}
    self:frame{
        hint = 'Encode a match state, decode a large message off the frame thread, or feed broken bytes.',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 520, align = 'stretch', gap = 12,
                ui.button{id = 'small', text = 'Encode and decode a state', align = 'stretch', onClick = function() self:roundTrip() end},
                ui.button{id = 'large', text = 'Decode 20000 frames off the frame thread', align = 'stretch', onClick = function() self:decodeLarge() end},
                ui.button{id = 'broken', text = 'Decode broken bytes', align = 'stretch', onClick = function() self:decodeBroken() end},
                ui.label{font = 'monospace', text = "net.encodeMessagePack(value)\nnet.decodeMessagePack(bytes)\nnet.decodeMessagePackAsync(bytes)"},
            },
            ui.scroll{grow = 1, ui.label{id = 'log', text = '', grow = 1}},
        }},
    }
    self:roundTrip()
    self:decodeLarge()
    self:decodeBroken()
end

function MessagePack:note(line)
    table.insert(self.lines, 1, line)
    self.lines[13] = nil
    self:set('log', {text = table.concat(self.lines, '\n')})
end

function MessagePack:roundTrip()
    local bytes = net.encodeMessagePack({tick = 812, players = {{id = 1, x = 40.5, hp = 3}, {id = 2, x = -12, hp = 0}}, over = false})
    local state = net.decodeMessagePack(bytes)
    self:note(string.format('Encoded the state in %d bytes and read tick %d with %d players, the first at x %.1f.', #bytes, state.tick, #state.players, state.players[1].x))
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
        self:note(string.format('Decoded %d bytes into %d frames off the frame thread in %d ms.', #bytes, #value.frames, datetime.now():millis() - started))
    end)
end

function MessagePack:decodeBroken()
    for _, bytes in ipairs({'\147\1', '\7\7', '\212\1\2', '\193'}) do
        local ok, message = pcall(net.decodeMessagePack, bytes)
        self:note(ok and 'The broken bytes decoded, which is a failure.' or message:gsub('^.-:%d+: ', ''))
    end
end

return MessagePack
