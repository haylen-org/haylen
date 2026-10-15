-- The wallet tab: the balance with a way to add funds, the cards and the latest movements.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Rides = require('rides')
local Shell = require('shell')

local Wallet = haylen.class('Wallet', Page)

Wallet.amounts = {{id = '10', text = '$10'}, {id = '20', text = '$20'}, {id = '50', text = '$50'}, {id = '100', text = '$100'}}

function Wallet:enter()
    self.amount = 20
    local cards = {}
    for _, card in ipairs(Rides.wallet.cards) do
        cards[#cards + 1] = {id = card.id, text = card.text, caption = card.caption, image = Shell.icon(card.icon)}
    end
    cards[#cards + 1] = {id = 'cash', text = 'Cash', caption = 'Pay the driver at the end of the ride', image = Shell.icon('cash')}
    self:mountGui(Shell.frame('wallet', ui.column{grow = 1, gap = 0,
        ui.row{padding = {14, 20, 8, 20}, ui.label{text = 'Wallet', font = 'title'}},
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 860, align = 'center', gap = 14, padding = {4, 20, 28, 20},
                ui.card{gap = 6, padding = {22, 24}, style = {colors = {raised = 'accent', border = 'accent'}},
                    ui.label{text = 'Wallet balance', color = 'onAccent'},
                    ui.label{id = 'balance', text = '', font = 'title', color = 'onAccent'},
                    ui.button{id = 'addFunds', text = 'Add funds', icon = Shell.icon('plus'), align = 'start', margin = {8, 0, 0, 0}, style = {colors = {raised = '#FF0B1020', border = '#FF0B1020', text = '#FFEAF0FF'}}, iconColor = 'accent', onClick = function(event)
                        event.gui:set('fundsDialog', {open = true})
                    end},
                },
                ui.sectionTitle{text = 'Payment methods'},
                ui.card{padding = 6, ui.list{id = 'cards', items = cards}},
                ui.sectionTitle{text = 'Movements'},
                ui.card{padding = 6, ui.list{id = 'movements', items = {}}},
            },
        },
        ui.dialog{id = 'fundsDialog', title = 'Add funds', message = 'The money comes from the card ending in 4821.',
            buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'add', text = 'Add funds', variant = 'primary'}},
            onAnswer = function(event)
                if event.button == 'add' then
                    Rides.addFunds(self.amount)
                    Shell.notify(Rides.money(self.amount) .. ' added to your wallet.')
                end
            end,
            ui.segmentedControl{id = 'amount', items = Wallet.amounts, selected = '20', onChange = function(event)
                self.amount = tonumber(event.value)
            end},
        },
    }))
    events.on('walletChanged', function()
        self:showWallet()
    end, {owner = self})
    events.on('tripChanged', function()
        self:showWallet()
    end, {owner = self})
    self:showWallet()
end

function Wallet:showWallet()
    self.gui:set('balance', {text = Rides.money(Rides.wallet.balance)})
    local movements = {}
    for index, move in ipairs(Rides.wallet.transactions) do
        movements[index] = {id = move.id, text = move.title, caption = move.caption .. ' · ' .. move.amount, image = Shell.icon(move.icon)}
    end
    self.gui:set('movements', {items = movements})
end

return Wallet
