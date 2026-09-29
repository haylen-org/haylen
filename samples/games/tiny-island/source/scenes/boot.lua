-- The first screen: it picks the language of the player on the first launch and preloads the menu, whose load brings in what the menus need.
local graphics2d = require('haylen.graphics2d')
local localization = require('haylen.localization')
local platform = require('haylen.platform')
local scene = require('haylen.scene')
local stored = require('haylen.preferences')

local preferences = require('systems.preferences')

local boot = {}
boot.__index = boot

function boot.new()
    return setmetatable({}, boot)
end

function boot:enter()
    scene.spawn(self, function()
        if not stored.has('language') then
            local locale = platform.call('system.locale'):await()
            local language = locale and localization.findBestMatch(locale)
            if language then
                preferences.set('language', language)
                preferences.save()
            end
        end

        self.menu = require('scenes.menu').new()
        local loaded, failure = scene.preload(self.menu):await()
        if not loaded then
            error(failure, 0)
        end
        scene.replace(self.menu, {duration = 0.8, color = '#FF1B1E2B'})
    end)
end

function boot:render()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    local width = 720
    local x = area.x + (area.width - width) / 2
    local y = area.y + area.height * 0.62
    local progress = self.menu and scene.loadProgress(self.menu) or 0
    graphics2d.drawRect(area, '#FF1B1E2B')
    graphics2d.drawText(nil, localization.text('title'), area.x + area.width / 2, y - 90, {size = 96, color = '#FFF2E3C6', anchor = {0.5, 0.5}})
    graphics2d.drawRect({x, y, width, 18}, '#FF3A3F55')
    graphics2d.drawRect({x, y, width * progress, 18}, '#FFF2C14E')
end

return boot
