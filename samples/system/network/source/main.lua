-- Haylen Network: one scene per way of talking to a server, HTTP requests with Varn's http module and WebSockets with haylen.net, picked from a menu.
local scene = require('haylen.scene')

scene.push(require('scenes.menu')())
