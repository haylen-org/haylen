-- The app of the gamepads check of the web: it logs what the first two gamepad slots hold whenever that changes, a free slot or the face button that the gamepad in it holds down, while `haylen test --target web` drives the Gamepad API of the browser with gamepads that connect, hold different buttons, disconnect and connect again.
local input = require('haylen.input')
local log = require('haylen.log')
local scene = require('haylen.scene')

local kButtons = {'south', 'east', 'west', 'north'}

local Check = {}

function Check.describe(slot)
    if not input.gamepadConnected(slot) then
        return 'free'
    end

    for _, button in ipairs(kButtons) do
        if input.gamepadDown(button, slot) then
            return button
        end
    end

    return 'idle'
end

function Check:enter()
    log.info('The gamepads check is ready.')
end

function Check:update()
    local seen = string.format('slot 1 %s, slot 2 %s', Check.describe(1), Check.describe(2))
    if seen ~= self.seen then
        self.seen = seen
        log.info(string.format('The gamepads check sees %s.', seen))
    end
end

scene.push(Check)
