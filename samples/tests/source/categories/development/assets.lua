-- Assets reload while the app runs: JSON updates in place and publishes "assetReloaded", the texts of a folder that "localization.loadFolder" loaded rebuild from their changed file, and an asset of a type that cannot update in place publishes "assetChanged" so the app loads it again. The content shows the values in cards and the events of each save in a list.
local assets = require('haylen.assets')
local events = require('haylen.events')
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local DevelopmentTest = require('categories.development.development-test')

local Assets = haylen.class('Assets', DevelopmentTest)

Assets.code = [[
self.values = assets.json('development/settings.json')
localization.loadFolder('development/texts')
events.on('assetReloaded', function(event)
    self.values = assets.json('development/settings.json')  -- Saved JSON reloads in place.
end, {owner = self})
localization.text('development.greeting')]]

Assets.file = 'content/development/settings.json and content/development/texts/en.json'
Assets.steps = {'Change the "label" or the "speed" of "settings.json", or the text of "texts/en.json".', 'Save the file.', 'The value changes at once, and the list shows the asset events of the save.'}
Assets.settings = 'development/settings.json'
Assets.events = 8

function Assets:init(entry)
    Assets.super.init(self, entry)
    self.heard = {}
    self.shown = {}
    self.count = 0
end

function Assets:enter()
    localization.loadFolder('development/texts')
    self.values = assets.json(Assets.settings)
    Assets.super.enter(self)
    for _, name in ipairs({'assetReloaded', 'assetChanged'}) do
        events.on(name, function(event)
            self.count = self.count + 1
            table.insert(self.heard, 1, {id = 'event-' .. self.count, text = string.format('The event "%s" for "%s"', name, event.path), caption = string.format('A %s asset, at %s', event.type, os.date('%H:%M:%S'))})
            self.heard[Assets.events + 1] = nil
            if event.path == Assets.settings then
                self.values = assets.json(Assets.settings)
            end
            self:set('events', {items = self.heard, visible = true})
            self:set('quiet', {visible = false})
        end, {owner = self})
    end
end

function Assets:content()
    return {
        ui.row{wrap = true, gap = 16, alignItems = 'stretch',
            DevelopmentTest.valueCard('label', 'The "label" of "settings.json"', ''),
            DevelopmentTest.valueCard('speed', 'The "speed" of "settings.json"', ''),
            DevelopmentTest.valueCard('greeting', 'The text "development.greeting"', ''),
        },
        ui.card{gap = 12, align = 'stretch', grow = 1,
            ui.label{text = 'Asset events', font = 'heading'},
            ui.emptyState{id = 'quiet', title = 'No asset event yet', message = 'Save one of the files and its events show here.'},
            ui.list{id = 'events', items = {}, visible = false},
        },
    }
end

-- Shows the values once they change.
function Assets:update(dt)
    Assets.super.update(self, dt)
    local values = {label = tostring(self.values.label), speed = tostring(self.values.speed), greeting = localization.text('development.greeting')}
    for id, text in pairs(values) do
        if self.shown[id] ~= text then
            self.shown[id] = text
            self:set(id, {text = text})
        end
    end
end

return Assets
