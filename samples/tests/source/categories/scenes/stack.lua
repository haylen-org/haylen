-- Stack and hooks: the operations of `haylen.scene` on the cards above this test, with the stack and the last hooks each scene received listed next to the buttons. The test sits on the menus of the project, so "Pop to the test" is the `popTo` that "popToRoot" would be in an app of its own.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Card = require('categories.scenes.card')
local SceneTest = require('categories.scenes.scene-test')
local Test = require('harness.test')

local Stack = haylen.class('Stack', SceneTest)

Stack.colors = {'#FF2E5E8A', '#FF8A3E5E', '#FF3E8A5E', '#FF8A6E2E', '#FF5E3E8A'}
Stack.transitions = {
    none = {},
    slide = {arrive = {effect = 'slideIn', direction = 'left', duration = 0.45, ease = 'quadOut'}, leave = {effect = 'slideIn', direction = 'right', duration = 0.45, ease = 'quadOut'}},
    fade = {arrive = {effect = 'fade', duration = 0.5}, leave = {effect = 'fade', duration = 0.5}},
}
Stack.lines = 12

function Stack:init(entry)
    Stack.super.init(self, entry)
    self.title = 'This test'
    self.history = {}
    self.cards = 0
    self.transition = Stack.transitions.slide
end

function Stack:operation(id, text, action)
    return ui.button{id = id, text = text, onClick = function()
        if not scene.transitioning() then
            action()
        end
    end}
end

function Stack:enter(params)
    self.depth = scene.size()
    self:frame{
        hint = 'Change the stack with the buttons. Escape or the B button pops the top card, and Back returns to the list once this test is on top again.',
        controls = {
            ui.formField{label = 'Transition', ui.combo{id = 'transition', selected = 'slide', items = {{id = 'none', text = 'Instant'}, {id = 'slide', text = 'Slide'}, {id = 'fade', text = 'Fade'}}, onChange = function(event)
                self.transition = Stack.transitions[event.value]
            end}},
            ui.grid{columns = 2, gap = 8, children = {
                self:operation('push', 'Push', function() scene.push(self:card(), self:arrival()) end),
                self:operation('pop', 'Pop', function() if scene.size() > self.depth then scene.pop(self.transition.leave) end end),
                self:operation('replace', 'Replace', function() if scene.size() > self.depth then scene.replace(self:card(), self:arrival()) end end),
                self:operation('popFirst', 'Pop to number 2', function() if scene.size() > self.depth + 1 then scene.popTo(self.depth + 1, self.transition.leave) end end),
                self:operation('popTest', 'Pop to the test', function() if scene.size() > self.depth then scene.popTo(self.depth, self.transition.leave) end end),
            }},
            ui.label{id = 'stack', text = '', font = 'monospace'},
            ui.label{id = 'log', text = '', font = 'caption', color = 'textMuted'},
        },
        focus = 'push',
    }
    self:record(self.title, 'enter')
end

function Stack:card()
    self.cards = self.cards + 1
    return Card({title = 'Card ' .. self.cards, caption = 'The "params" of its change say where it came from', color = Stack.colors[self.cards % #Stack.colors + 1], leave = self.transition.leave, listener = function(card, hook, detail)
        self:record(card.title, hook, detail)
    end})
end

-- The options of a change that brings a card: the chosen transition and `params` that reach its `load` and `enter` hooks.
function Stack:arrival()
    local options = {params = 'from depth ' .. (scene.size() - self.depth + 1)}
    for key, value in pairs(self.transition.arrive or {}) do
        options[key] = value
    end
    return options
end

-- Adds a hook to the list, newest first, with the `params` it received, and shows the scenes from this test up as they are now.
function Stack:record(name, hook, detail)
    table.insert(self.history, 1, string.format('%.2f  %s: "%s"%s', haylen.elapsed(), name, hook, detail and ' (' .. tostring(detail) .. ')' or ''))
    self.history[Stack.lines + 1] = nil
    local names = {}
    local list = scene.list()
    for index = self.depth or #list, #list do
        names[#names + 1] = (index - self.depth + 1) .. '. ' .. list[index].title
    end
    self:set('stack', {text = 'Stack from this test up:\n' .. table.concat(names, '\n')})
    self:set('log', {text = table.concat(self.history, '\n')})
    self:set('status', {text = string.format('Scenes above the menus %d   Transitioning "%s"', #list - self.depth + 1, scene.transitioning())})
end

function Stack:exit()
    self:record(self.title, 'exit')
    Stack.super.exit(self)
end

function Stack:pause()
    self:record(self.title, 'pause')
end

function Stack:resume()
    self:record(self.title, 'resume')
end

function Stack:exitTransitionStarted()
    self:record(self.title, 'exitTransitionStarted')
end

function Stack:enterTransitionFinished()
    self:record(self.title, 'enterTransitionFinished')
end

function Stack:draw(area)
    graphics2d.drawRect(area, '#FF1C2230')
    Test.caption('This test, under its cards', area.width / 2, area.height / 2, {size = 64, anchor = {0.5, 0.5}, color = '#FF5A6A8A'})
end

return Stack
