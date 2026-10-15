-- The saved tab: the listings the person saved with the heart.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Saved = require('saved')
local Shell = require('shell')
local Widgets = require('widgets')

local SavedPage = haylen.class('SavedPage', Page)

function SavedPage:enter()
    self:mountGui(Shell.frame('saved', ui.column{grow = 1, gap = 0,
        ui.row{padding = {14, 20, 8, 20}, ui.label{text = 'Saved', font = 'title'}},
        ui.collection{id = 'saved', grow = 1, layout = 'grid', minCellSize = 230, cellAspect = 1.42, gap = 14, padding = {4, 20, 24, 20}, rememberFocus = true,
            types = {listing = Widgets.listingCard()},
            onSelect = function(event)
                Shell.open(require('screens.listing'), {listing = event.item})
            end,
            ui.emptyState{image = 'images/hero.png', imageSize = 220, title = 'Nothing saved yet', message = 'Tap the heart of a listing to keep it here.'},
        },
    }))
    self.list = self.gui:collection('saved')
    self.list:setItems(Saved.list)
    events.on('savedChanged', function()
        self.list:setItems(Saved.list)
    end, {owner = self})
end

return SavedPage
