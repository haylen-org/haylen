-- A transparent scene over an events test with a card in the middle of the screen that closes with its button or a cancel.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Overlay = haylen.class('Overlay', scene.Scene)

Overlay.transparent = true
Overlay.transition = {effect = 'crossFade', duration = 0.25}

-- Pushes the overlay with a cross fade and the `params` it enters with.
function Overlay:open(params)
    if not scene.transitioning() then
        scene.push(self, {effect = Overlay.transition.effect, duration = Overlay.transition.duration, params = params})
    end
end

-- Mounts the card with a `title`, the nodes of `children` and a Close button, all owned by the overlay so they end when it unloads.
function Overlay:card(title, children)
    local nodes = {ui.label{text = title, font = 'heading'}}
    for _, child in ipairs(children) do
        nodes[#nodes + 1] = child
    end
    nodes[#nodes + 1] = ui.button{id = 'close', text = 'Close', variant = 'primary', onClick = function() self:close() end}
    self.document = ui.mount(ui.card{anchor = 'center', width = 760, gap = 16, onCancel = function() self:close() end, children = nodes}, {owner = self, layer = 1})
    self.document:command('close', 'focus')
end

function Overlay:close()
    if not scene.transitioning() then
        scene.pop(Overlay.transition)
    end
end

function Overlay:render()
    graphics2d.beginScreen()
    graphics2d.drawRect(graphics2d.canvasBounds(), '#A0000000')
end

return Overlay
