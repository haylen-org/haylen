-- What every test of the sample shares: the page with the Back button, the title, the description and the hint line, the way to and from the menu, the sockets and listeners a test opens, and the formatting of times, sizes and bodies.
local datetime = require('datetime')
local haylen = require('haylen')
local json = require('json')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = {}

-- The fade that every change between the menu and a test plays.
sample.transition = {effect = 'fade', duration = 0.3, color = '#FF101418'}

function sample.open(entry)
    if not scene.transitioning() then
        scene.push(require(entry.module)(entry), sample.transition)
    end
end

function sample.back()
    if not scene.transitioning() then
        scene.pop(sample.transition)
    end
end

-- The wall clock in milliseconds, which times requests and round trips.
function sample.millis()
    return datetime.now():millis()
end

function sample.clock()
    return os.date('%H:%M:%S')
end

-- Formats a byte count for people, such as 512 bytes or 3.4 KB.
function sample.bytes(count)
    if count < 1024 then
        return string.format('%d bytes', count)
    end
    if count < 1024 * 1024 then
        return string.format('%.1f KB', count / 1024)
    end
    return string.format('%.2f MB', count / (1024 * 1024))
end

-- Shows a body as indented JSON when it is JSON, and as it came otherwise.
function sample.body(text)
    local ok, value = pcall(json.decode, text)
    if ok and type(value) == 'table' then
        return json.encode(value, {pretty = true})
    end
    return text
end

-- Formats binary data as hexadecimal bytes.
function sample.hex(data)
    return (data:gsub('.', function(byte)
        return string.format('%02X ', byte:byte())
    end))
end

-- The base of every test scene. A test sets its hints and the control that takes the focus, and returns the nodes of its page from content.
local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

Test.hints = ''
Test.focus = 'back'

function Test:init(entry)
    self.entry = entry
    self.sockets = {}
    self.connections = {}
end

function Test:content()
    return {}
end

-- Mounts the page, which the scene owns, so it goes away when the scene unloads. The Back button, Escape, the east gamepad button and the Menu button of a TV remote return to the menu.
function Test:enter()
    window.setBackLeavesApp(false)
    self.document = ui.mount(ui.column{
        padding = 24,
        gap = 16,
        onCancel = sample.back,
        ui.row{gap = 24,
            ui.button{id = 'back', text = 'Back', align = 'start', onClick = sample.back},
            ui.column{grow = 1, gap = 4,
                ui.label{text = self.entry.title, font = 'heading'},
                ui.label{text = self.entry.description, color = 'textMuted'},
            },
        },
        ui.row{grow = 1, gap = 24, children = self:content()},
        ui.label{text = self.hints, font = 'caption', color = 'textMuted'},
    }, {owner = self})
    self.document:command(self.focus, 'focus')
end

-- Sockets report on their own, not through the scene, so leaving the test stops its listeners and closes its sockets.
function Test:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
    for _, socket in ipairs(self.sockets) do
        socket:close()
    end
    window.setBackLeavesApp(true)
end

-- Keeps a socket the test opened, so leaving the test closes it.
function Test:keep(socket)
    self.sockets[#self.sockets + 1] = socket
    return socket
end

-- Listens to an event of a socket until the test ends.
function Test:on(socket, event, listener)
    self.connections[#self.connections + 1] = socket:on(event, listener)
end

function Test:show(id, properties)
    self.document:set(id, properties)
end

return sample
