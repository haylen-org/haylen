-- The menu: a drawer that the app presents from the start of the screen over the dimmed map, with the rider, the trips, the wallet, the account and, where quitting closes the app, a button that quits it. A tap on the map beside it, Escape and the back button close it.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Rides = require('rides')
local Shell = require('shell')
local Style = require('style')

local Menu = haylen.class('Menu', ui.Screen)

Menu.drawer = {metrics = {panelRadius = 0}}

-- The drawer is the root of the menu, so the presentation keeps the taps on it and closes the menu for the taps on the map beside it.
function Menu:build()
    local space = Style.space
    local items = {
        {id = 'activity', text = 'Your trips', caption = Rides.thousands(#Rides.past) .. ' trips', image = Shell.icon('receipt')},
        {id = 'wallet', text = 'Wallet', caption = Rides.money(Rides.wallet.balance) .. ' in your wallet', image = Shell.icon('wallet')},
        {id = 'account', text = 'Account', caption = 'Safety, contacts and the invite', image = Shell.icon('user')},
    }
    return ui.panel{anchor = 'stretchLeft', anchorTo = 'screen', width = 420, padding = space.xxs, style = Menu.drawer,
        ui.safeArea{
            ui.column{gap = space.m, padding = {space.l, space.l},
                ui.row{gap = space.m,
                    ui.avatar{name = 'Rosa Almeida', size = 72},
                    ui.column{gap = 0, grow = 1,
                        ui.label{text = 'Rosa Almeida', font = 'heading', wrap = false},
                        ui.row{gap = space.xxs, ui.icon{image = Shell.icon('star'), size = 20, color = 'accentStrong'}, ui.label{text = '4.96 as a rider', font = 'caption', color = 'textMuted'}},
                    },
                },
                ui.divider{},
                ui.list{id = 'pages', items = items, autofocus = true, onSelect = function(event)
                    Shell.openPage(require('screens.' .. event.item))
                end},
                ui.spacer{grow = 1},
                -- Where quitting closes the app, the menu offers it, so the way the app ends can be tried.
                haylen.canQuit() and ui.button{id = 'quit', text = 'Quit the app', align = 'start', onClick = function() haylen.quit() end} or nil,
            },
        },
    }
end

return Menu
