-- A module reloads in place: the scene keeps the module it required, the counter it counts and its place on the stack, while the next call of the module runs the new code.
local haylen = require('haylen')

local DevelopmentTest = require('categories.development.development-test')
local Test = require('harness.test')
local greeting = require('categories.development.edit.greeting')

local ModuleReload = haylen.class('ModuleReload', DevelopmentTest)

ModuleReload.file = 'source/categories/development/edit/greeting.lua'
ModuleReload.steps = {'Change the text that "greeting.text" returns, or the color of "greeting.color".', 'Save the file.', 'The greeting changes at once, while the counter keeps counting, because the module reloaded in place and the app did not restart.'}

function ModuleReload:init(entry)
    ModuleReload.super.init(self, entry)
    self.seconds = 0
end

function ModuleReload:update(dt)
    ModuleReload.super.update(self, dt)
    self.seconds = self.seconds + dt
end

function ModuleReload:draw(area)
    Test.caption(greeting.text(self.seconds), area.width / 2, area.height / 2 - 40, {size = 48, color = greeting.color(), anchor = {0.5, 0.5}, maxWidth = area.width - 80})
    Test.caption(string.format('Counter %.1f', self.seconds), area.width / 2, area.height / 2 + 60, {size = 32, color = Test.ink, anchor = {0.5, 0.5}})
end

return ModuleReload
