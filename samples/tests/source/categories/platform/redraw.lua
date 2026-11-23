-- Redraw: the app updates and draws in every frame or only in the frames that something changed in, and the test counts the frames it drew in the last second, draws a mark for each one, slides a box with a tween, turns a dial whose own code asks for its frames and gives back the redraw of the project when it ends.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')

local Redraw = haylen.class('Redraw', Test)

Redraw.modes = {{id = 'always', text = 'Always'}, {id = 'onChange', text = 'On change'}}
Redraw.span = 2
Redraw.margin = 48
Redraw.turnsPerSecond = 0.5

Redraw.code = [[
window.setRedraw('onChange')  -- Only frames that change draw.
window.requestRedraw()        -- Code that moves by itself asks.
print(window.redraw())]]

function Redraw:enter()
    self.previous = window.redraw()
    self.drawn = {}
    self.box = {x = 0}
    self.angle = 0
    self.turning = false
    self:frame{
        hint = 'Pick when the app draws. On change, the marks stop while nothing moves, and a slide, the dial or any input draws them again.',
        focus = 'redraw',
        controls = {
            ui.formField{label = 'Redraw', ui.segmentedControl{id = 'redraw', items = Redraw.modes, selected = self.previous, onChange = function(event)
                window.setRedraw(event.value)
            end}},
            ui.button{id = 'slide', text = 'Slide the box', onClick = function() self:slide() end},
            ui.toggle{id = 'dial', text = 'Turn the dial', checked = false, onChange = function(event) self.turning = event.checked end},
        },
    }
    timer.every(0.5, function() self:report() end, {owner = self})
end

function Redraw:exit()
    window.setRedraw(self.previous)
    Redraw.super.exit(self)
end

-- The box slides across and back with a tween, which keeps an app that redraws on change drawing until it ends.
function Redraw:slide()
    tween.to(self.box, 0.8, {x = self.box.x < 0.5 and 1 or 0}, {ease = 'cubicInOut', owner = self})
end

-- Every update is a frame that draws, which the test marks by the real time it played at. The dial moves by its own code, so it asks for the next frame while it turns.
function Redraw:update(dt)
    Redraw.super.update(self, dt)
    local drawn = self.drawn
    drawn[#drawn + 1] = haylen.elapsed()
    if self.turning then
        self.angle = self.angle + dt * Redraw.turnsPerSecond * math.pi * 2
        window.requestRedraw()
    end
end

-- A timer tells the count twice a second, so the line stays current while the app draws nothing in between.
function Redraw:report()
    local now = haylen.elapsed()
    local drawn = self.drawn
    while drawn[1] and drawn[1] < now - Redraw.span do
        table.remove(drawn, 1)
    end
    local recent = 0
    for _, at in ipairs(drawn) do
        if at >= now - 1 then
            recent = recent + 1
        end
    end
    local mode = window.redraw() == 'always' and 'in every frame' or 'only when something changes'
    self:status(string.format('The app draws %s and drew %d frames in the last second.', mode, recent))
end

function Redraw:draw(area)
    local margin = Redraw.margin
    local width = area.width - margin * 2
    local lane = area.height * 0.3
    graphics2d.drawRect({margin, lane - 4, width, 8}, Test.line)
    graphics2d.drawShape({margin + self.box.x * (width - 72), lane - 36, 72, 72}, {radius = 12, color = Test.accent})

    local center = {area.width * 0.5, area.height * 0.55}
    local radius = math.min(area.height * 0.12, 80)
    graphics2d.drawRing(center[1], center[2], radius, 4, Test.line)
    graphics2d.drawLine(center[1], center[2], center[1] + math.cos(self.angle) * radius, center[2] + math.sin(self.angle) * radius, 6, Test.warm)

    -- Each mark is one frame that drew, placed by its time, so the frames that an app that redraws on change skips leave gaps.
    local strip = area.height * 0.82
    local now = haylen.elapsed()
    for _, at in ipairs(self.drawn) do
        local x = margin + width - (now - at) / Redraw.span * width
        if x >= margin then
            graphics2d.drawRect({x - 1.5, strip - 32, 3, 64}, Test.green)
        end
    end
    Test.caption('Each mark is a frame that drew in the last two seconds.', margin, strip + 48, {color = Test.muted})
end

return Redraw
