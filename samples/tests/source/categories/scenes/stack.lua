-- Stack and hooks: the operations of `haylen.scene` on the cards above this test, with the hooks each scene received on a timeline next to the cards. Each card is a transparent panel over a part of the play area, cascaded by its depth, so the stack shows as cards over cards while the buttons of the test stay usable. The test sits on the menus of the project, so "Pop to the test" is the `popTo` that "popToRoot" would be in an app of its own.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Card = require('categories.scenes.card')
local SceneTest = require('categories.scenes.scene-test')
local Test = require('harness.test')

local Stack = haylen.class('Stack', SceneTest)

Stack.code = [[
scene.push(card, {effect = 'slideIn', direction = 'left', duration = 0.45, params = 'from depth 2'})
scene.replace(card, {effect = 'fade', duration = 0.5})
scene.pop({effect = 'slideIn', direction = 'right', duration = 0.45})
scene.popTo(2)  -- Or scene.popToRoot().
print(scene.size(), scene.top().title, #scene.list(), scene.transitioning())]]

Stack.transitions = {
    none = {},
    slide = {arrive = {effect = 'slideIn', direction = 'left', duration = 0.45, ease = 'quadOut'}, leave = {effect = 'slideIn', direction = 'right', duration = 0.45, ease = 'quadOut'}},
    fade = {arrive = {effect = 'fade', duration = 0.5}, leave = {effect = 'fade', duration = 0.5}},
}
Stack.lines = 14
-- The colors of the hooks on the timeline.
Stack.hookColors = {load = Test.violet, enter = Test.green, exit = Test.red, unload = Test.muted, pause = Test.warm, resume = Test.accent, exitTransitionStarted = Test.warm, enterTransitionFinished = Test.accent}

function Stack:init(entry)
    Stack.super.init(self, entry)
    self.title = 'This test'
    self.history = {}
    self.cards = 0
    self.started = haylen.elapsed()
    self.transition = Stack.transitions.slide
end

function Stack:operation(id, text, action)
    return ui.button{id = id, text = text, align = 'stretch', onClick = function()
        if not scene.transitioning() then
            action()
        end
    end}
end

function Stack:enter(params)
    self.depth = scene.size()
    self:frame{
        hint = 'Change the stack with the buttons. Escape or the B button pops the top card, and Back returns to the list once this test is on top again.',
        code = Stack.code,
        controls = {
            ui.formField{label = 'Transition', ui.combo{id = 'transition', selected = 'slide', items = {{id = 'none', text = 'Instant'}, {id = 'slide', text = 'Slide'}, {id = 'fade', text = 'Fade'}}, onChange = function(event)
                self.transition = Stack.transitions[event.value]
            end}},
            self:operation('push', 'Push', function() scene.push(self:card(), self:arrival()) end),
            self:operation('pop', 'Pop', function() if scene.size() > self.depth then scene.pop(self.transition.leave) end end),
            self:operation('replace', 'Replace', function() if scene.size() > self.depth then scene.replace(self:card(), self:arrival()) end end),
            self:operation('popFirst', 'Pop to the first card', function() if scene.size() > self.depth + 1 then scene.popTo(self.depth + 1, self.transition.leave) end end),
            self:operation('popTest', 'Pop to the test', function() if scene.size() > self.depth then scene.popTo(self.depth, self.transition.leave) end end),
        },
        focus = 'push',
    }
    self:record(self.title, 'enter')
end

-- The place of a card at a depth above the test, where the test itself is depth 0: a panel in the left part of the play area, moved down and right by its depth.
function Stack:cardRect(depth)
    local stage = self.stage
    local width, height = stage.width * 0.5, stage.height * 0.62
    local step = math.min(46, (stage.height - height - 48) / 5)
    local offset = math.min(depth, 5) * step
    return m.rect(stage.x + 24 + offset, stage.y + 24 + offset, width, height)
end

function Stack:card()
    self.cards = self.cards + 1
    local depth = scene.size() - self.depth + 1
    return Card({title = 'Card ' .. self.cards, caption = 'Pushed at depth ' .. depth, night = self.cards % 2 == 0, leave = self.transition.leave, stage = self:cardRect(depth), listener = function(card, hook, detail)
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

-- Adds a hook to the timeline, newest first, with the time since the test opened and the `params` the hook received, and shows the stack in the status line, since the test does not update while cards cover it.
function Stack:record(name, hook, detail)
    table.insert(self.history, 1, {time = haylen.elapsed() - self.started, name = name, hook = hook, detail = detail})
    self.history[Stack.lines + 1] = nil
    if self.gui then
        local cards = scene.size() - self.depth
        Test.setNode(self, 'status', {text = string.format('%d %s above the test, and the last hook was "%s".', cards, cards == 1 and 'card is' or 'cards are', hook)})
    end
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

-- Draws the place of the stack, where the test itself is the bottom card, and the timeline of the hooks on the right.
function Stack:draw(area)
    local bottom = self:cardRect(0)
    graphics2d.drawShape({bottom.x - self.stage.x, bottom.y - self.stage.y, bottom.width, bottom.height}, {radius = 18, color = '#FF232938', borderWidth = 3, borderColor = Test.line})
    graphics2d.drawText(nil, 'This test, under its cards', bottom.x - self.stage.x + bottom.width / 2, bottom.y - self.stage.y + bottom.height / 2, {size = 30, color = Test.muted, anchor = {0.5, 0.5}, layer = 1})

    local left = area.width * 0.5 + 24 + 5 * 46
    local log = {math.min(left, area.width - 420), 24, 0, area.height - 48}
    log[3] = area.width - log[1] - 24
    graphics2d.drawShape(log, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    graphics2d.drawText(nil, 'Hooks, newest first', log[1] + 20, log[2] + 18, {size = 22, color = Test.muted, layer = 2})
    local lineHeight = math.min(36, (log[4] - 70) / Stack.lines)
    for index, entry in ipairs(self.history) do
        local y = log[2] + 60 + (index - 1) * lineHeight
        if y + lineHeight > log[2] + log[4] then
            break
        end
        local color = Stack.hookColors[entry.hook] or Test.ink
        graphics2d.drawText(nil, string.format('%.2f s', entry.time), log[1] + 96, y, {size = 20, color = Test.muted, anchor = {1, 0}, layer = 2})
        graphics2d.drawShape({log[1] + 110, y + 6, 12, 12}, {kind = 'ellipse', color = color, layer = 2})
        graphics2d.drawText(nil, entry.name .. ': ' .. entry.hook .. (entry.detail and ', ' .. tostring(entry.detail) or ''), log[1] + 132, y, {size = 20, color = Test.ink, maxWidth = log[3] - 150, wrap = false, minScale = 0.7, layer = 2})
    end
end

return Stack
