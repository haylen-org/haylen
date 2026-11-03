-- The prompts of the input tests: a binding of the action map written for people, such as "Space", "Left stick" or the letter on the bottom button of the gamepad in hand, and drawn as a chip in the shape of what the player presses: a key cap for keys, a round button for gamepads with the letter, shape or place of a face button, a pill for the mouse and an outlined pill for touch controls. The statuses of the tests also take from it the last device and the actions held in words.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')

local prompts = {}

prompts.keys = {space = 'Space', leftShift = 'Shift', rightShift = 'Shift', leftControl = 'Ctrl', rightControl = 'Ctrl', leftAlt = 'Alt', rightAlt = 'Alt', enter = 'Enter', escape = 'Esc', tab = 'Tab', backspace = 'Backspace', up = 'Up', down = 'Down', left = 'Left', right = 'Right'}
prompts.faces = {
    letters = {south = 'A', east = 'B', west = 'X', north = 'Y'},
    swappedLetters = {south = 'B', east = 'A', west = 'Y', north = 'X'},
    shapes = {south = 'Cross', east = 'Circle', west = 'Square', north = 'Triangle'},
    unknown = {south = 'South', east = 'East', west = 'West', north = 'North'},
}
prompts.faceColors = {south = '#FF4CD45A', east = '#FFF0473E', west = '#FF2F86F6', north = '#FFFFC93C'}
prompts.buttons = {dpadUp = 'D-pad up', dpadDown = 'D-pad down', dpadLeft = 'D-pad left', dpadRight = 'D-pad right', leftShoulder = 'LB', rightShoulder = 'RB', start = 'Start', back = 'View', guide = 'Guide', leftStick = 'L3', rightStick = 'R3'}
prompts.axes = {leftTrigger = 'LT', rightTrigger = 'RT', leftX = 'Left stick', leftY = 'Left stick', rightX = 'Right stick', rightY = 'Right stick'}
prompts.mouse = {left = 'Left click', right = 'Right click', middle = 'Middle click'}
prompts.ink = '#FF15172A'
prompts.devices = {keyboardMouse = 'the keyboard and the mouse', gamepad = 'a gamepad', touch = 'the touch screen'}

-- Says where the last input came from, such as "the keyboard and the mouse", for the status of a test.
function prompts.lastDevice()
    return prompts.devices[input.lastDevice()]
end

-- Says which of the actions `names` the player holds, such as "nothing is held" or "jump and dash are held", for the status of a test.
function prompts.held(names)
    local held = {}
    for _, name in ipairs(names) do
        if input.down(name) then
            held[#held + 1] = name
        end
    end
    if #held == 0 then
        return 'nothing is held'
    end
    return table.concat(held, ' and ') .. (#held == 1 and ' is held' or ' are held')
end

-- The labels of the face buttons of the first connected gamepad, or their places when none is connected or its labels are unknown, the way the key captures of the engine write them.
function prompts.faceLabels()
    for index = 1, 8 do
        if input.gamepadConnected(index) then
            return prompts.faces[input.gamepadLabels(index)]
        end
    end
    return prompts.faces.unknown
end

-- Returns the text and the kind of a binding, such as "Space" and "key" for "key:space".
function prompts.describe(binding)
    local source, name = binding:match('^(%w+):([%w%+%-]+)$')
    if source == 'key' then
        return prompts.keys[name] or name:upper(), 'key'
    elseif source == 'button' then
        return prompts.faceLabels()[name] or prompts.buttons[name] or name, 'button', name
    elseif source == 'axis' then
        return prompts.axes[name:match('^%a+')] or name, 'button'
    elseif source == 'stick' then
        return (name == 'left' and 'Left' or 'Right') .. ' stick', 'button'
    elseif source == 'mouse' then
        return prompts.mouse[name] or name, 'mouse'
    end
    return 'Touch ' .. name:gsub('^%l', string.upper), 'touch'
end

function prompts.font()
    prompts.labelFont = prompts.labelFont or assets.font('fonts/lilita-one-regular.ttf')
    return prompts.labelFont
end

-- Draws the chip of a binding with its left edge at `x` and its middle at `y`, `size` units tall, and returns its width.
function prompts.chip(binding, x, y, size, layer)
    local text, kind, face = prompts.describe(binding)
    layer = layer or 4
    local font = prompts.font()
    local textSize = size * 0.5
    local width = math.max(size, graphics2d.measureText(font, text, {size = textSize}) + size * 0.7)
    local top = y - size / 2
    if kind == 'key' then
        graphics2d.drawShape({x, top + size * 0.1, width, size * 0.9}, {radius = size * 0.2, color = '#FFB9C0E0', layer = layer})
        graphics2d.drawShape({x, top, width, size * 0.86}, {radius = size * 0.2, color = '#FFF2F3FF', borderWidth = 2, borderColor = prompts.ink, layer = layer})
        graphics2d.drawText(font, text, x + width / 2, top + size * 0.43, {size = textSize, color = prompts.ink, anchor = {0.5, 0.5}, layer = layer + 1})
    elseif kind == 'button' then
        local fill = face and prompts.faceColors[face] or '#FF3A4066'
        graphics2d.drawShape({x, top, width, size}, {radius = size / 2, color = fill, borderWidth = 2, borderColor = prompts.ink, layer = layer})
        graphics2d.drawText(font, text, x + width / 2, y, {size = textSize, color = '#FFFFFFFF', anchor = {0.5, 0.5}, outlineWidth = 2, outlineColor = prompts.ink, layer = layer + 1})
    elseif kind == 'mouse' then
        graphics2d.drawShape({x, top, width, size}, {radius = size / 2, color = '#FF5D6596', borderWidth = 2, borderColor = prompts.ink, layer = layer})
        graphics2d.drawText(font, text, x + width / 2, y, {size = textSize, color = '#FFFFFFFF', anchor = {0.5, 0.5}, layer = layer + 1})
    else
        graphics2d.drawShape({x, top, width, size}, {radius = size / 2, color = '#00000000', borderWidth = 2, borderColor = '#FF8FB0FF', layer = layer})
        graphics2d.drawText(font, text, x + width / 2, y, {size = textSize, color = '#FF8FB0FF', anchor = {0.5, 0.5}, layer = layer + 1})
    end
    return width
end

-- Draws the chips of a list of bindings in a row from `x`, wrapping at `right`, and returns the bottom of the last row.
function prompts.chips(bindings, x, y, size, right, layer)
    local left, gap = x, size * 0.25
    for _, binding in ipairs(bindings) do
        local text = prompts.describe(binding)
        local width = math.max(size, graphics2d.measureText(prompts.font(), text, {size = size * 0.5}) + size * 0.7)
        if left > x and left + width > right then
            left, y = x, y + size + gap
        end
        prompts.chip(binding, left, y, size, layer)
        left = left + width + gap
    end
    return y + size / 2
end

return prompts
