-- The activity tab: the 520 past trips in a recycled list under sticky months, each opening its details.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Rides = require('rides')
local Shell = require('shell')

local Activity = haylen.class('Activity', Page)

function Activity:enter()
    self:mountGui(Shell.frame('activity', ui.column{grow = 1, gap = 0,
        ui.row{padding = {14, 20, 8, 20}, ui.label{text = 'Activity', font = 'title'}},
        Shell.centered(980, ui.collection{id = 'trips', gap = 2, padding = {0, 12, 24, 12}, rememberFocus = true,
            types = {
                month = {template = ui.panel{padding = {12, 8, 6, 8}, style = Shell.flatBar, ui.sectionTitle{part = 'title', bind = {text = 'title'}}}, sticky = true, interactive = false},
                trip = {template = ui.row{gap = 14, padding = {8, 8},
                    ui.image{part = 'art', bind = {image = 'art'}, width = 88, height = 52, fit = 'contain'},
                    ui.column{gap = 0, grow = 1,
                        ui.label{part = 'title', bind = {text = 'title'}, font = 'button', wrap = false},
                        ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false},
                    },
                    ui.label{part = 'price', bind = {text = 'price'}, font = 'button'},
                }, estimatedSize = 76},
            },
            onSelect = function(event)
                Shell.open(require('screens.trip-detail'), {trip = event.item})
            end,
        }, 1),
    }))
    self.list = self.gui:collection('trips')
    events.on('tripChanged', function()
        self:showTrips()
    end, {owner = self})
    self:showTrips()
end

-- The trips with a header before the first trip of every month.
function Activity:showTrips()
    local items, month = {}, nil
    for _, trip in ipairs(Rides.past) do
        if trip.month ~= month then
            month = trip.month
            items[#items + 1] = {id = 'month-' .. #items, type = 'month', title = month}
        end
        items[#items + 1] = trip
    end
    self.list:setItems(items)
end

return Activity
