-- The account tab: the profile, the look of the app, the notices, the saved addresses and the ways to pay.
local haylen = require('haylen')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

local Account = require('data.account')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local AccountPage = haylen.class('AccountPage', Page)

function AccountPage:enter()
    local addresses, payments = {}, {}
    for _, address in ipairs(Account.addresses) do
        addresses[#addresses + 1] = {id = address.id, text = address.text, caption = address.line}
    end
    for _, payment in ipairs(Account.payments) do
        payments[#payments + 1] = {id = payment.id, text = payment.text}
    end
    self:mountGui(Shell.frame('account', ui.column{grow = 1, gap = 0,
        ui.row{gap = 12, padding = {14, 16, 8, 20}, ui.label{text = 'Account', font = 'title', grow = 1}, Widgets.cartButton(self)},
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 760, align = 'center', gap = 14, padding = {4, 20, 28, 20},
                ui.card{
                    ui.row{gap = 16,
                        ui.avatar{name = Account.name, size = 72},
                        ui.column{gap = 2, grow = 1, ui.label{text = Account.name, font = 'heading'}, ui.label{text = Account.email, color = 'textMuted'}},
                        ui.button{id = 'edit', icon = Shell.icon('edit'), variant = 'icon', tooltip = 'Edit the profile', accessibilityLabel = 'Edit the profile', onClick = function()
                            Shell.notify('Editing the profile is not part of this sample.', 'information')
                        end},
                    },
                },
                ui.settingsForm{
                    ui.sectionTitle{text = 'Look'},
                    ui.settingsRow{label = 'Dark theme', caption = 'Warm dark colors for the evening.', ui.toggle{id = 'dark', checked = ui.theme() == 'foodDark', onChange = function(event)
                        preferences.set('dark', event.checked)
                        ui.setTheme(event.checked and 'foodDark' or 'foodLight')
                        Shell.showTab('account')
                    end}},
                    ui.sectionTitle{text = 'Notices'},
                    ui.settingsRow{label = 'Order updates', caption = 'When the kitchen cooks and the courier rides.', ui.toggle{id = 'updates', checked = true}},
                    ui.settingsRow{label = 'Offers', caption = 'Deals of the restaurants near you.', ui.toggle{id = 'offers', checked = false}},
                },
                ui.sectionTitle{text = 'Saved addresses'},
                ui.card{padding = 6, ui.list{id = 'addresses', items = addresses, selected = Account.address, onSelect = function(event)
                    Account.address = event.item
                    Shell.notify('Deliveries now go to ' .. Account.currentAddress().text .. '.')
                end}},
                ui.sectionTitle{text = 'Payment'},
                ui.card{padding = 6, ui.list{id = 'payments', items = payments}},
                ui.button{id = 'signOut', text = 'Sign out', icon = Shell.icon('logout'), variant = 'destructive', align = 'start', onClick = function(event)
                    event.gui:set('confirmSignOut', {open = true})
                end},
            },
        },
        ui.dialog{id = 'confirmSignOut', title = 'Sign out?', message = 'This sample keeps you signed in, so nothing changes.',
            buttons = {{id = 'stay', text = 'Stay'}, {id = 'out', text = 'Sign out', variant = 'destructive'}}},
    }))
    Widgets.showCount(self.gui)
end

return AccountPage
