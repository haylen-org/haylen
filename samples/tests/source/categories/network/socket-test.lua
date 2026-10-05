-- The base of the tests of the category that open sockets. Sockets report on their own, not through the scene, so leaving the test stops their listeners and closes them.
local haylen = require('haylen')

local Test = require('harness.test')

local SocketTest = haylen.class('SocketTest', Test)

function SocketTest:init(entry)
    SocketTest.super.init(self, entry)
    self.sockets = {}
    self.connections = {}
end

function SocketTest:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
    for _, socket in ipairs(self.sockets) do
        socket:close()
    end
    SocketTest.super.exit(self)
end

-- Keeps a socket the test opened, so leaving the test closes it.
function SocketTest:keep(socket)
    self.sockets[#self.sockets + 1] = socket
    return socket
end

-- Listens to an event of a socket until the test ends.
function SocketTest:on(socket, event, listener)
    self.connections[#self.connections + 1] = socket:on(event, listener)
end

return SocketTest
