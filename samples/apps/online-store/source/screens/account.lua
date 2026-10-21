-- The account tab: the person, the notices, the address, help and, where quitting closes the app, a button that quits it.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Orders = require('orders')
local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')
local Style = require('style')
local Wishlist = require('wishlist')

local Account = haylen.class('Account', Page)

function Account:enter()
    local space = Style.space
    local address = Store.me.address
    self:mountGui(Shell.frame(self, 'account', ui.scroll{grow = 1, height = 0,
        Shell.centered(Style.size.reading, ui.column{gap = space.l, padding = {space.l, space.l, space.xxl, space.l},
            ui.row{gap = space.m,
                ui.avatar{name = Store.me.name, size = 88},
                ui.column{gap = 0, grow = 1, ui.label{text = Store.me.name, font = 'title', style = Style.text('title', Style.type.heading)}, ui.label{text = Store.me.email, color = 'textMuted'}},
            },
            ui.row{gap = space.s, align = 'stretch',
                self:stat(#Orders.list, 'Orders', 'orders'),
                self:stat(#Wishlist.list, 'Saved', 'saved'),
            },
            ui.label{text = 'Notices', font = 'heading', margin = {space.s, 0, 0, 0}},
            ui.card{gap = 0, padding = {space.xs, space.l},
                ui.settingsRow{label = 'Order updates', caption = 'When an order ships and arrives.', ui.toggle{id = 'updates', checked = true, accessibilityLabel = 'Order updates'}},
                ui.divider{},
                ui.settingsRow{label = 'Sales and new arrivals', caption = 'A short note every week.', ui.toggle{id = 'news', checked = false, accessibilityLabel = 'Sales and new arrivals'}},
            },
            ui.label{text = 'Address', font = 'heading', margin = {space.s, 0, 0, 0}},
            ui.card{gap = space.xxs, padding = space.l,
                ui.row{gap = space.m, ui.icon{image = Shell.icon('pin'), size = 30, color = 'textMuted'}, ui.column{gap = 0, grow = 1, ui.label{text = address.name, font = 'button'}, ui.label{text = address.street .. ', ' .. address.city .. ' ' .. address.zip, color = 'textMuted'}}},
            },
            ui.label{text = 'Help', font = 'heading', margin = {space.s, 0, 0, 0}},
            ui.card{padding = space.xs, ui.list{id = 'help', items = {
                {id = 'returns', text = 'Returns and refunds', caption = 'Free for 30 days, in the box it came in.', image = Shell.icon('return')},
                {id = 'sizes', text = 'Size guide', caption = 'Measures for clothes and shoes.', image = Shell.icon('ruler')},
                {id = 'contact', text = 'Contact us', caption = 'Every day from 8 to 20.', image = Shell.icon('chat')},
            }, onSelect = function(event)
                Shell.notify(({returns = 'Start a return from the page of an order.', sizes = 'Every product page lists its sizes.', contact = 'Write to help@example.com.'})[event.item])
            end}},
            -- Where quitting closes the app, the account offers it, so the way the app ends can be tried.
            haylen.canQuit() and ui.button{id = 'quit', text = 'Quit the app', align = 'start', onClick = function() haylen.quit() end} or nil,
        }),
    }, Shell.phoneBar(self, 'Account')))
end

-- A count of the person with the tab it opens.
function Account:stat(count, text, tab)
    return ui.button{id = 'stat-' .. tab, text = count .. ' ' .. text, grow = 1, height = 72, onClick = function()
        Shell.showTab(tab)
    end}
end

return Account
