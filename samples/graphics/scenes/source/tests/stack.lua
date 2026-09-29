-- Stack and hooks: the operations of haylen.scene on a stack whose bottom is this test, with the stack and the last hooks each scene received listed next to the buttons.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Card = require('card')
local sample = require('sample')

local Stack = haylen.class('Stack', sample.Test)

Stack.hints = 'Change the stack with the buttons. Escape or the B button pops the top card, and Back returns to the menu from any depth.'

Stack.colors = {'#FF2E5E8A', '#FF8A3E5E', '#FF3E8A5E', '#FF8A6E2E', '#FF5E3E8A'}
Stack.transitions = {
    none = {},
    slide = {arrive = {effect = 'slideIn', direction = 'left', duration = 0.45, ease = 'quad_out'}, leave = {effect = 'slideIn', direction = 'right', duration = 0.45, ease = 'quad_out'}},
    fade = {arrive = {effect = 'fade', duration = 0.5}, leave = {effect = 'fade', duration = 0.5}},
}

function Stack:init(entry)
    Stack.super.init(self, entry)
    self.name = 'Stack test'
    self.lines = {}
    self.cards = 0
    self.transition = Stack.transitions.slide
end

function Stack:controls()
    local function operation(text, action)
        return ui.button{text = text, onClick = function()
            if not scene.transitioning() then
                action()
            end
        end}
    end
    return {
        ui.formField{label = 'Transition', ui.combo{selected = 'slide', items = {{id = 'none', text = 'Instant'}, {id = 'slide', text = 'Slide'}, {id = 'fade', text = 'Fade'}}, onChange = function(event)
            self.transition = Stack.transitions[event.value]
        end}},
        ui.grid{columns = 2, gap = 8, children = {
            operation('Push', function() scene.push(self:card(), self:arrival()) end),
            operation('Pop', function() if scene.size() > 1 then scene.pop(self.transition.leave) end end),
            operation('Replace', function() if scene.size() > 1 then scene.replace(self:card(), self:arrival()) end end),
            operation('Pop to 2', function() scene.popTo(2, self.transition.leave) end),
            operation('Pop to root', function() scene.popToRoot(self.transition.leave) end),
        }},
        ui.label{id = 'stack', text = '', font = 'monospace'},
        ui.label{id = 'log', text = '', font = 'caption', color = 'textMuted'},
    }
end

function Stack:card()
    self.cards = self.cards + 1
    return Card({title = 'Card ' .. self.cards, caption = 'the params of its change say where it came from', color = Stack.colors[self.cards % #Stack.colors + 1], leave = self.transition.leave, listener = function(card, hook, detail)
        self:record(card.title, hook, detail)
    end})
end

-- The options of a change that brings a card: the chosen transition and params that reach its load and enter hooks.
function Stack:arrival()
    local options = {params = 'from depth ' .. scene.size()}
    for key, value in pairs(self.transition.arrive or {}) do
        options[key] = value
    end
    return options
end

-- Adds a hook to the log, newest first, with the params it received, and shows the stack as it is now.
function Stack:record(name, hook, detail)
    table.insert(self.lines, 1, string.format('%.2f  %s: %s%s', haylen.time(), name, hook, detail and ' (' .. tostring(detail) .. ')' or ''))
    self.lines[13] = nil
    local names = {}
    for index, entry in ipairs(scene.list()) do
        names[#names + 1] = index .. '. ' .. (entry and (entry.title or entry.name) or 'a scene from C++')
    end
    self.header:set('stack', {text = 'Stack, bottom to top:\n' .. table.concat(names, '\n')})
    self.header:set('log', {text = table.concat(self.lines, '\n')})
    self:setStatus(string.format('%d scenes on the stack, transitioning %s', scene.size(), tostring(scene.transitioning())))
end

function Stack:enter(params)
    Stack.super.enter(self)
    self:record(self.name, 'enter')
end

function Stack:exit()
    self:record(self.name, 'exit')
end

function Stack:pause()
    self:record(self.name, 'pause')
end

function Stack:resume()
    self:record(self.name, 'resume')
end

function Stack:exitTransitionStarted()
    self:record(self.name, 'exitTransitionStarted')
end

function Stack:enterTransitionFinished()
    self:record(self.name, 'enterTransitionFinished')
end

function Stack:render()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect(area, '#FF1C2230')
    graphics2d.drawText(nil, 'The bottom of the stack', area.x + area.width * 0.4, area.y + area.height * 0.55, {size = 64, anchor = {0.5, 0.5}, color = '#FF5A6A8A'})
end

return Stack
