-- The trips: the 520 past trips in a recycled list under sticky months, each opening its details.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Rides = require('rides')
local Shell = require('shell')
local Style = require('style')

local Activity = haylen.class('Activity', Page)

function Activity:enter()
    local space = Style.space
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Your trips'),
        Shell.centered(Style.size.reading, ui.collection{id = 'trips', gap = 0, padding = {0, space.s, space.xl, space.s}, rememberFocus = true,
            types = {
                month = {template = ui.panel{padding = {space.m, space.s, space.xs, space.s}, style = {metrics = {panelRadius = 0}}, ui.label{part = 'title', bind = {text = 'title'}, font = 'heading'}}, sticky = true, interactive = false},
                trip = {template = ui.row{gap = space.m, padding = {space.s, space.s},
                    ui.panel{width = 112, height = 72, padding = space.xs, style = {colors = {panel = Style.color.mist}, metrics = {panelRadius = Style.radius.control}}, ui.image{part = 'art', bind = {image = 'art'}, fit = 'contain', align = 'stretch', grow = 1}},
                    ui.column{gap = 0, grow = 1,
                        ui.label{part = 'title', bind = {text = 'title'}, font = 'button', wrap = false},
                        ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false},
                    },
                    ui.label{part = 'price', bind = {text = 'price'}, font = 'button'},
                }, estimatedSize = 96},
            },
            onSelect = function(event)
                Shell.open(require('screens.trip-detail'), {trip = event.item})
            end,
        }, 1),
    })
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
