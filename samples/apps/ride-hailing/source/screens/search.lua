-- The search of a destination: the pickup, a field that narrows the 565 places of the city as the person types, and the saved places first.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Places = require('data.places')
local Shell = require('shell')

local Search = haylen.class('Search', Page)

function Search:enter()
    self.query = ''
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Where to?'),
        Shell.centered(860, ui.column{gap = 0, padding = {0, 16, 0, 16},
            ui.card{gap = 8, padding = 12,
                ui.row{gap = 12, ui.icon{image = Shell.icon('pin'), size = 26, color = 'accentText'}, ui.label{text = Places.here.name .. ', ' .. Places.here.line, color = 'textMuted', grow = 1, wrap = false}},
                ui.divider{},
                ui.row{gap = 12, ui.icon{image = Shell.icon('flag'), size = 26, color = 'text'}, ui.filterField{id = 'query', placeholder = 'Search a place or an address', grow = 1, returnKey = 'search', onChange = function(event)
                    self.query = string.lower(event.value)
                    self:refresh()
                end, onSubmit = function()
                    local first = self.shown[1]
                    if first then
                        self:pick(first.id)
                    end
                end}},
            },
            ui.label{id = 'count', text = '', font = 'caption', color = 'textMuted', margin = {12, 8, 4, 8}},
        }),
        Shell.centered(860, ui.collection{id = 'places', gap = 2, padding = {0, 16, 24, 16}, rememberFocus = true,
            types = {place = {template = ui.row{gap = 14, padding = {8, 8},
                ui.icon{part = 'icon', bind = {image = 'icon'}, image = Shell.icon('pin'), size = 28, color = 'textMuted'},
                ui.column{gap = 0, grow = 1,
                    ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
                    ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false},
                },
            }, estimatedSize = 76}},
            onSelect = function(event)
                self:pick(event.item)
            end,
            ui.emptyState{image = Shell.icon('search'), imageSize = 72, title = 'No place matches', message = 'Try a street or another name.'},
        }, 1),
    })
    self.places = self.gui:collection('places')
    self:refresh()
    self.gui:command('query', 'focus')
end

function Search:refresh()
    local shown = {}
    for _, saved in ipairs(Places.saved) do
        local place = Places.find(saved.id)
        if self.query == '' or string.find(place.searchText, self.query, 1, true) then
            place.icon = Shell.icon(saved.icon)
            shown[#shown + 1] = place
        end
    end
    for _, place in ipairs(Places.list) do
        if self.query == '' or string.find(place.searchText, self.query, 1, true) then
            shown[#shown + 1] = place
        end
    end
    self.shown = shown
    self.places:setItems(shown)
    self.gui:set('count', {text = #shown == 1 and '1 place' or #shown .. ' places'})
end

function Search:pick(id)
    Shell.replace(require('screens.preview'), {to = id})
end

return Search
