-- The search of a destination: the pickup and the destination joined by a line, a field that narrows the 565 places of the city as the person types, and the saved places first.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Places = require('data.places')
local Shell = require('shell')
local Style = require('style')

local Search = haylen.class('Search', Page)

Search.backTransition = {duration = 0.24, effect = 'push', direction = 'down', ease = 'cubicOut'}

function Search:enter()
    local space = Style.space
    self.query = ''
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Plan your ride'),
        Shell.centered(Style.size.reading, ui.column{gap = 0, padding = {space.m, space.l, space.s, space.l},
            ui.row{gap = space.m, align = 'stretch',
                ui.column{gap = 0, width = 28, padding = {22, 0},
                    ui.icon{image = Shell.icon('dot'), size = 28, color = 'text', align = 'center'},
                    ui.divider{vertical = true, grow = 1, width = 4, align = 'center', color = 'borderStrong', style = {metrics = {borderWidth = 2.5}}},
                    ui.icon{image = Shell.icon('square'), size = 28, color = 'text', align = 'center'},
                },
                ui.column{gap = space.s, grow = 1,
                    ui.button{id = 'pickup', text = Places.here.name .. ', ' .. Places.here.line, justify = 'start', align = 'stretch', style = {colors = {raised = Style.color.mist, border = Style.color.mist}, fonts = {button = {bold = false}}}, onClick = function()
                        Shell.notify('Your pickup follows where you are.')
                    end},
                    ui.filterField{id = 'query', placeholder = 'Where to?', returnKey = 'search', style = {colors = {raised = Style.color.mist}, metrics = {borderWidth = 2}}, onChange = function(event)
                        self.query = string.lower(event.value)
                        self:refresh()
                    end, onSubmit = function()
                        local first = self.shown[1]
                        if first then
                            self:pick(first.id)
                        end
                    end},
                },
            },
            ui.label{id = 'count', text = '', font = 'caption', color = 'textMuted', margin = {space.m, 0, 0, 0}},
        }),
        Shell.centered(Style.size.reading, ui.collection{id = 'places', gap = 0, padding = {0, space.s, space.xl, space.s}, rememberFocus = true,
            types = {place = {template = ui.row{gap = space.m, padding = {space.s, space.s},
                ui.panel{width = 52, height = 52, padding = space.s, justify = 'center', style = {colors = {panel = Style.color.mist}, metrics = {panelRadius = 26}},
                    ui.icon{part = 'icon', bind = {image = 'icon'}, image = Shell.icon('pin'), size = 28, color = 'text', align = 'center'},
                },
                ui.column{gap = 0, grow = 1,
                    ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
                    ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false},
                },
            }, estimatedSize = 80}},
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
