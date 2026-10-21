-- A transparent scene over an events test with a card that closes with its button or a cancel, in the middle of the screen over a dimmed test, or at a place the scene gives it over a test that stays clear.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Overlay = haylen.class('Overlay', scene.Scene)

Overlay.transparent = true
Overlay.transition = {effect = 'crossFade', duration = 0.25}
Overlay.dim = true

-- Pushes the overlay with a cross fade and the `params` it enters with.
function Overlay:open(params)
    if not scene.transitioning() then
        scene.push(self, {effect = Overlay.transition.effect, duration = Overlay.transition.duration, params = params})
    end
end

-- Mounts the card with a `title`, the nodes of `children` and a Close button, all owned by the overlay so they end when it unloads. The optional `place` gives the card the `anchor`, the `margin` and the `width` of a place of its own.
function Overlay:card(title, children, place)
    local nodes = {ui.label{text = title, font = 'heading'}}
    for _, child in ipairs(children) do
        nodes[#nodes + 1] = child
    end
    nodes[#nodes + 1] = ui.button{id = 'close', text = 'Close', variant = 'primary', onClick = function() self:close() end}
    place = place or {anchor = 'center', width = 760}
    self.gui = ui.mount(ui.card{anchor = place.anchor, margin = place.margin, width = place.width, gap = 16, onCancel = function() self:close() end, children = nodes}, {owner = self, layer = 1})
    self.gui:command('close', 'focus')
end

function Overlay:close()
    if not scene.transitioning() then
        scene.pop(Overlay.transition)
    end
end

function Overlay:render()
    if self.dim then
        graphics2d.beginScreen()
        graphics2d.drawRect(graphics2d.canvasBounds(), '#A0000000')
    end
end

return Overlay
