-- Building blocks shared by the screens, so every button sounds the same and every caption has the same outline.
local ui = require('haylen.ui')

local sound = require('systems.sound')

local widgets = {}

-- Returns a translation that the UI resolves every frame, so a language change shows at once.
function widgets.text(key, arguments)
    return {key = key, args = arguments}
end

function widgets.button(id, key, action, options)
    options = options or {}
    return ui.button{id = id, text = widgets.text(key), variant = options.variant or 'primary', align = 'stretch', grow = options.grow, onClick = function()
        sound.play(options.sound or 'click')
        action()
    end}
end

-- White text with a dark outline, readable over the island.
function widgets.caption(id, text)
    return ui.label{id = id, text = text, font = 'heading', color = 'onAccent', outline = '#FF1B1E2B', outlineWidth = 3, textAlign = 'center', align = 'center'}
end

return widgets
