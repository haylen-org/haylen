-- The base of the scene tests, which push cards, overlays and menus above themselves: the action map that pops them, and a way back to the list only while the test itself is on top, so a cancel that reaches its frame under them does nothing.
local haylen = require('haylen')
local scene = require('haylen.scene')

local Test = require('harness.test')

local SceneTest = haylen.class('SceneTest', Test)

SceneTest.actions = {actions = {
    {name = 'back', type = 'button', bindings = {'key:escape', 'button:east'}},
    {name = 'pause', type = 'button', bindings = {'key:p', 'button:north'}},
}}

function SceneTest:frame(options)
    self:loadActions(SceneTest.actions)
    SceneTest.super.frame(self, options)
end

function SceneTest:cancel()
    if scene.top() == self then
        SceneTest.super.cancel(self)
    end
end

return SceneTest
