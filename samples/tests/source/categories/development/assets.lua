-- Assets reload while the app runs: JSON updates in place and publishes "assetReloaded", the texts of a folder that "localization.loadFolder" loaded rebuild from their changed file, and an asset of a type that cannot update in place publishes "assetChanged" so the app loads it again.
local assets = require('haylen.assets')
local events = require('haylen.events')
local haylen = require('haylen')
local localization = require('haylen.localization')

local DevelopmentTest = require('categories.development.development-test')
local Test = require('harness.test')

local Assets = haylen.class('Assets', DevelopmentTest)

Assets.file = 'content/development/settings.json and content/development/texts/en.json'
Assets.steps = {'Change the "label" or the "speed" of "settings.json", or the text of "texts/en.json".', 'Save the file.', 'The value changes at once, and the list shows the asset events of the save.'}
Assets.settings = 'development/settings.json'

function Assets:init(entry)
    Assets.super.init(self, entry)
    self.heard = {}
end

function Assets:enter()
    Assets.super.enter(self)
    localization.loadFolder('development/texts')
    self.values = assets.json(Assets.settings)
    for _, name in ipairs({'assetReloaded', 'assetChanged'}) do
        events.on(name, function(event)
            table.insert(self.heard, 1, string.format('%s, %s "%s"', name, event.type, event.path))
            self.heard[6] = nil
            if event.path == Assets.settings then
                self.values = assets.json(Assets.settings)
            end
        end, {owner = self})
    end
end

function Assets:draw(area)
    local rows = {
        {'The "label" of settings.json', tostring(self.values.label)},
        {'The "speed" of settings.json', tostring(self.values.speed)},
        {'The text "development.greeting"', localization.text('development.greeting')},
    }
    for index, row in ipairs(rows) do
        local y = 40 + (index - 1) * 80
        Test.caption(row[1], 60, y, {size = 26, color = Test.muted})
        Test.caption(row[2], 60, y + 34, {size = 28, color = Test.ink})
    end
    Test.caption('Asset events', 60, 300, {size = 26, color = Test.muted})
    Test.caption(#self.heard > 0 and table.concat(self.heard, '\n') or 'None yet.', 60, 334, {size = 22, color = Test.ink, maxWidth = area.width - 120})
end

return Assets
