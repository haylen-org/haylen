-- The account tab: the profile, the dark theme, the notices, the address and help.
local haylen = require('haylen')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')
local Widgets = require('widgets')

local Account = haylen.class('Account', Page)

function Account:enter()
    local address = Store.me.address
    self:mountGui(Shell.frame('account', ui.column{grow = 1, gap = 0,
        ui.row{gap = 12, padding = {14, 16, 8, 20}, ui.label{text = 'Account', font = 'title', grow = 1}, Widgets.bagButton(self)},
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 760, align = 'center', gap = 14, padding = {4, 20, 28, 20},
                ui.card{
                    ui.row{gap = 16,
                        ui.avatar{name = Store.me.name, size = 72},
                        ui.column{gap = 2, grow = 1, ui.label{text = Store.me.name, font = 'heading'}, ui.label{text = Store.me.email, color = 'textMuted'}},
                    },
                },
                ui.settingsForm{
                    ui.sectionTitle{text = 'Look'},
                    ui.settingsRow{label = 'Dark theme', caption = 'Dark colors for the evening.', ui.toggle{id = 'dark', checked = ui.theme() == 'storeDark', onChange = function(event)
                        preferences.set('dark', event.checked)
                        ui.setTheme(event.checked and 'storeDark' or 'storeLight')
                    end}},
                    ui.sectionTitle{text = 'Notices'},
                    ui.settingsRow{label = 'Order updates', caption = 'When an order ships and arrives.', ui.toggle{id = 'updates', checked = true}},
                    ui.settingsRow{label = 'Sales and new arrivals', caption = 'A short note every week.', ui.toggle{id = 'news', checked = false}},
                },
                ui.sectionTitle{text = 'Address'},
                ui.card{ui.label{text = address.name}, ui.label{text = address.street .. ', ' .. address.city .. ' ' .. address.zip, color = 'textMuted'}},
                ui.sectionTitle{text = 'Help'},
                ui.card{padding = 6, ui.list{id = 'help', items = {{id = 'returns', text = 'Returns and refunds', image = Shell.icon('truck')}, {id = 'sizes', text = 'Size guide', image = Shell.icon('ruler')}, {id = 'contact', text = 'Contact us', image = Shell.icon('chat')}}, onSelect = function()
                    Shell.notify('Help pages are not part of this sample.', 'information')
                end}},
            },
        },
    }))
    Widgets.showCount(self.gui)
end

return Account
