-- The module of the test "DEV-004". Change the title and save the file while the test runs: the test rebuilds its interface in its "reloaded" hook with the new title, the kept signal stays the same object, and its listener never doubles, because the reload ends the connection of the previous load.
local hotReload = require('haylen.hotReload')
local signal = require('haylen.signal')

local hud = hotReload.keep('hud', function()
    return {pressed = signal.new('development.hud'), presses = 0, loads = 0}
end)

hud.loads = hud.loads + 1
hud.pressed:connect(function()
    hud.presses = hud.presses + 1
end)

function hud.title()
    return 'Scoreboard of the first version'
end

return hud
