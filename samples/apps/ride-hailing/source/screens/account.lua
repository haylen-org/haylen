-- The account: the rider, an invitation with the illustration of the app, the safety options and the trusted contacts.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Style = require('style')

local Account = haylen.class('Account', Page)

function Account:enter()
    local space = Style.space
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Account'),
        ui.scroll{grow = 1, height = 0,
            Shell.centered(Style.size.reading, ui.column{gap = space.l, padding = {space.l, space.l, space.xl, space.l},
                ui.row{gap = space.m,
                    ui.avatar{name = 'Rosa Almeida', size = 88},
                    ui.column{gap = space.xxs, grow = 1, ui.label{text = 'Rosa Almeida', font = 'title', style = Style.text('title', Style.type.heading)}, ui.label{text = '4.96 stars as a rider · 214 trips', color = 'textMuted'}},
                },
                ui.card{padding = {space.m, space.m, space.m, space.l}, style = {colors = {raised = Style.color.mist, border = Style.color.mist}, metrics = {panelRadius = Style.radius.sheet}},
                    ui.row{gap = space.s,
                        ui.column{gap = space.xxs, grow = 1,
                            ui.label{text = 'Invite a friend', font = 'heading'},
                            ui.label{text = 'Your friend gets a free ride across town, and so do you.', color = 'textMuted'},
                            ui.button{id = 'invite', text = 'Share the invite', icon = Shell.icon('gift'), iconColor = 'onTooltip', align = 'start', margin = {space.s, 0, 0, 0}, style = {colors = {raised = Style.color.ink, border = Style.color.ink, text = Style.color.paper}}, onClick = function()
                                Shell.notify('Your invite is ready to share.')
                            end},
                        },
                        ui.image{image = 'images/hero.png', width = 200, height = 150, fit = 'contain', accessibilityHidden = true},
                    },
                },
                ui.label{text = 'Safety', font = 'heading'},
                ui.column{gap = 0,
                    ui.settingsRow{label = 'Share every trip', caption = 'Trusted contacts follow your rides on a map.', ui.toggle{id = 'share', checked = false, accessibilityLabel = 'Share every trip'}},
                    ui.divider{},
                    ui.settingsRow{label = 'Quiet rides', caption = 'Ask drivers for a ride without small talk.', ui.toggle{id = 'quiet', checked = true, accessibilityLabel = 'Quiet rides'}},
                },
                ui.label{text = 'Trusted contacts', font = 'heading'},
                ui.list{id = 'contacts', items = {{id = 'tom', text = 'Tomas Almeida', caption = 'Brother', image = Shell.icon('user')}, {id = 'ines', text = 'Ines Kato', caption = 'Friend', image = Shell.icon('user')}}},
            }),
        },
    })
end

return Account
