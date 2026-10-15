-- The profile tab: the person as a seller, the dark theme and the listings the person published.
local haylen = require('haylen')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

local Market = require('data.market')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Profile = haylen.class('Profile', Page)

function Profile:enter()
    local me = Market.me
    self:mountGui(Shell.frame('profile', ui.column{grow = 1, gap = 0,
        Shell.centered(1100, ui.column{gap = 12, padding = {14, 20, 8, 20},
            ui.card{
                ui.row{gap = 16,
                    ui.avatar{name = me.name, size = 76},
                    ui.column{gap = 2, grow = 1,
                        ui.label{text = me.name, font = 'heading'},
                        ui.row{gap = 6, ui.icon{image = Shell.icon('star'), size = 20, color = 'warning'}, ui.label{text = string.format('%s · %d sales · %s', me.rating, me.sales, me.joined), font = 'caption', color = 'textMuted'}},
                    },
                },
                ui.divider{},
                ui.toggle{id = 'dark', text = 'Dark theme', checked = ui.theme() == 'marketDark', onChange = function(event)
                    preferences.set('dark', event.checked)
                    ui.setTheme(event.checked and 'marketDark' or 'marketLight')
                    Shell.showTab('profile')
                end},
            },
            ui.sectionTitle{text = string.format('Your listings, %d', #me.listings)},
        }),
        ui.collection{id = 'mine', grow = 1, layout = 'grid', minCellSize = 230, cellAspect = 1.42, gap = 14, padding = {4, 20, 24, 20}, rememberFocus = true,
            types = {listing = Widgets.listingCard()},
            onSelect = function(event)
                Shell.open(require('screens.listing'), {listing = event.item})
            end,
            ui.emptyState{image = Shell.icon('camera'), imageSize = 72, title = 'Nothing for sale yet', message = 'Publish an item from the sell tab and it shows here.'},
        },
    }))
    self.gui:collection('mine'):setItems(me.listings)
end

return Profile
