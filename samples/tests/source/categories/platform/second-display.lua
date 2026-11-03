-- Second display: an output of the app on a display other than the one of the window, which shows a scoreboard that the test draws into its render target every frame, with a preview of the target in the play area. A click on the window of the output drops a ring where it landed, and the events of the output mark the play area as they arrive.
local display = require('haylen.display')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Test = require('harness.test')

local SecondDisplay = haylen.class('SecondDisplay', Test)

SecondDisplay.code = "system.requestPermission('screens'):await()\nlocal board = display.open(other.id, {clearColor = '#FF000000'})\ncamera.x, camera.y = board.width / 2, board.height / 2\ngraphics2d.beginTarget(board.target, camera, {clear = '#FF14202C'})\ngraphics2d.drawText(nil, 'Score', board.width / 2, board.height / 2)\nevents.on('outputInput', function(info) print(info.type, info.x, info.y) end)"
SecondDisplay.ringSeconds = 1.2

function SecondDisplay:enter()
    self.output = nil
    self.time = 0
    self.rings = {}
    self.heard = {}
    self.boardCamera = graphics2d.newCamera()
    self:frame{
        hint = 'Connect a second display, or attach an external display to the iPad simulator, then open the scoreboard on it. Click its window on a desktop to drop rings.',
        focus = 'open',
        controls = {
            ui.button{id = 'open', text = 'Open on the next display', variant = 'primary', onClick = function() self:open() end},
            ui.button{id = 'close', text = 'Close the output', onClick = function() self:closeOutput() end},
            ui.label{id = 'answer', text = '', color = 'textMuted', font = 'caption'},
        },
    }
    for _, name in ipairs({'outputShown', 'outputHidden', 'outputResized', 'outputClosed'}) do
        events.on(name, function(info)
            table.insert(self.heard, 1, string.format('%s %s', name, info.ending or (info.width and string.format('%d by %d', info.width, info.height)) or ''))
            self.heard[6] = nil
        end, {owner = self})
    end
    events.on('outputInput', function(info)
        if info.type == 'mouseDown' and self.output and info.output == self.output.id then
            self.rings[#self.rings + 1] = {x = info.x * self.output.scale, y = info.y * self.output.scale, age = 0}
        end
    end)
end

-- A browser and an iPhone or iPad of iOS 27 show the app every display only once it holds the permission "screens", so the button asks first, inside the click that a browser needs.
function SecondDisplay:open()
    self:spawn(function()
        system.requestPermission('screens'):await()
        local current = display.current().id
        for _, shown in ipairs(display.list()) do
            if shown.id ~= current then
                self:closeOutput()
                self.output = display.open(shown.id, {title = 'Scoreboard', clearColor = '#FF000000'})
                self:set('answer', {text = string.format('Opening on "%s".', shown.name ~= '' and shown.name or shown.id)})
                return
            end
        end
        self:set('answer', {text = 'Only the display of the window is connected. An external display of an iPad shows up a moment after the first press.'})
    end, {owner = self})
end

function SecondDisplay:closeOutput()
    if self.output then
        self.output:close()
        self.output = nil
    end
end

function SecondDisplay:exit()
    self:closeOutput()
    SecondDisplay.super.exit(self)
end

function SecondDisplay:update(dt)
    SecondDisplay.super.update(self, dt)
    self.time = self.time + dt
    for index = #self.rings, 1, -1 do
        local ring = self.rings[index]
        ring.age = ring.age + dt
        if ring.age > SecondDisplay.ringSeconds then
            table.remove(self.rings, index)
        end
    end
    local output = self.output
    self:status(output and string.format('The output is %s, %d by %d pixels at %gx.', output.state, output.width, output.height, output.scale) or 'No output is open.')
end

-- The scoreboard counts the seconds and sweeps a light across the display, in the pixels of the target.
function SecondDisplay:drawBoard(width, height, scale)
    graphics2d.drawRect({0, 0, width, height}, '#FF14202C')
    local sweep = (self.time * 0.25 % 1) * width
    graphics2d.drawRect({sweep - 40 * scale, 0, 80 * scale, height}, '#3F8FB0FF')
    graphics2d.drawText(nil, 'Scoreboard', width / 2, height * 0.3, {size = 72 * scale, color = Test.ink, anchor = {0.5, 0.5}})
    graphics2d.drawText(nil, string.format('%d', math.floor(self.time)), width / 2, height * 0.58, {size = 220 * scale, color = Test.warm, anchor = {0.5, 0.5}})
    for _, ring in ipairs(self.rings) do
        local grown = ring.age / SecondDisplay.ringSeconds
        graphics2d.drawRing(ring.x, ring.y, (20 + 120 * grown) * scale, 6 * scale, string.format('#%02XF2B23A', math.floor((1 - grown) * 255)))
    end
end

function SecondDisplay:render()
    local output = self.output
    if output and output.shown then
        local camera = self.boardCamera
        camera.x, camera.y = output.width / 2, output.height / 2
        graphics2d.beginTarget(output.target, camera, {clear = '#FF14202C'})
        self:drawBoard(output.width, output.height, output.scale)
    end
    SecondDisplay.super.render(self)
end

-- The play area previews the target as large as it fits, or tells what the output waits for.
function SecondDisplay:draw(area)
    local output = self.output
    if output and output.shown then
        local fit = math.min((area.width - 96) / output.width, (area.height - 200) / output.height)
        local width, height = output.width * fit, output.height * fit
        local x, y = (area.width - width) / 2, 48
        graphics2d.draw(output.target.texture, x, y, {width = width, height = height, pivotX = 0, pivotY = 0, blend = 'premultiplied'})
        graphics2d.drawRectOutline({x, y, width, height}, 2, Test.accent)
    else
        Test.caption(output and string.format('The output is %s.', output.state) or 'Open the scoreboard on another display.', area.width / 2, area.height / 2 - 60, {size = 26, color = Test.ink, anchor = {0.5, 0.5}})
    end
    for index, line in ipairs(self.heard) do
        Test.caption(line, 48, area.height - 48 - index * 26 + 26, {color = index == 1 and Test.warm or Test.muted})
    end
end

return SecondDisplay
