-- A module reloads in place: the scene keeps the module it required, the counter it counts and its place on the stack, while the next call of the module runs the new code.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local DevelopmentTest = require('categories.development.development-test')
local Test = require('harness.test')
local greeting = require('categories.development.edit.greeting')

local ModuleReload = haylen.class('ModuleReload', DevelopmentTest)

ModuleReload.code = [[
local greeting = require('categories.development.edit.greeting')  -- Saving the file reloads it in place.
graphics2d.drawText(nil, greeting.text(self.seconds), x, y, {size = 44, color = greeting.color(), align = 'center'})
events.on('moduleReloaded', function(event)
    self.last = event.path
end, {owner = self})
hotReload.active()  -- On while the app runs in development.]]

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

-- The greeting of the module and the counter of the scene, centered together in a card.
function ModuleReload:draw(area)
    local width = math.min(area.width - 80, 1000)
    local x, y = area.width / 2, area.height / 2
    graphics2d.drawShape({x - width / 2, y - 150, width, 300}, {radius = 24, color = '#FF1F2535', borderWidth = 3, borderColor = '#FF39415A'})
    graphics2d.drawText(nil, 'The module says', x, y - 118, {size = 22, color = Test.muted, anchor = {0.5, 0}})
    graphics2d.drawText(nil, greeting.text(self.seconds), x, y - 20, {size = 44, color = greeting.color(), anchor = {0.5, 0.5}, align = 'center', maxWidth = width - 80})
    graphics2d.drawShape({x - 150, y + 66, 300, 56}, {radius = 28, color = '#FF262C3C', borderWidth = 2, borderColor = '#FF39415A'})
    graphics2d.drawText(nil, string.format('Counter %d', math.floor(self.seconds)), x, y + 94, {size = 30, color = Test.ink, anchor = {0.5, 0.5}})
end

return ModuleReload
