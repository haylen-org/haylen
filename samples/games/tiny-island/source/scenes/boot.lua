-- The first screen: it picks the language of the player on the first launch and preloads the menu, whose load brings in what the menus need.
local graphics2d = require('haylen.graphics2d')
local localization = require('haylen.localization')
local scene = require('haylen.scene')
local stored = require('haylen.preferences')
local system = require('haylen.system')

local art = require('systems.art')
local preferences = require('systems.preferences')

local boot = {}
boot.__index = boot

function boot.new()
    return setmetatable({logo = art.texture('ui/logo.png')}, boot)
end

function boot:enter()
    if not stored.has('language') then
        local locale = system.info().locale
        local language = locale and localization.findBestMatch(locale)
        if language then
            preferences.set('language', language)
            preferences.save()
        end
    end

    scene.spawn(self, function()
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
    local y = area.y + area.height * 0.74
    local progress = self.menu and scene.loadProgress(self.menu) or 0
    graphics2d.drawRect(area, '#FF1E2138')
    graphics2d.draw(self.logo, area.x + area.width / 2, y - 60, {pivotY = 1, scaleX = 0.65, scaleY = 0.65})
    -- The bar is a pill whose fill keeps round ends from its first dot, inset by the same 6 units on every side.
    graphics2d.drawShape({x - 6, y - 6, width + 12, 30}, {radius = 15, color = '#FF15172A'})
    if progress > 0 then
        graphics2d.drawShape({x, y, math.max(width * progress, 18), 18}, {radius = 9, color = '#FF2F86F6', layer = 1})
    end
end

return boot
