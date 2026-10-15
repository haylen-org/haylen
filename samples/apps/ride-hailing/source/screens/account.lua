-- The account tab: the profile, the look of the map, the safety options and the trusted contacts.
local haylen = require('haylen')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')

local Account = haylen.class('Account', Page)

function Account:enter()
    self:mountGui(Shell.frame('account', ui.column{grow = 1, gap = 0,
        ui.row{padding = {14, 20, 8, 20}, ui.label{text = 'Account', font = 'title'}},
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 760, align = 'center', gap = 14, padding = {4, 20, 28, 20},
                ui.card{
                    ui.row{gap = 16,
                        ui.avatar{name = 'Rosa Almeida', size = 72},
                        ui.column{gap = 2, grow = 1, ui.label{text = 'Rosa Almeida', font = 'heading'}, ui.label{text = '4.96 stars as a rider · 214 trips', color = 'textMuted'}},
                    },
                },
                ui.card{padding = {12, 12, 12, 22}, style = {colors = {raised = 'accentBackground'}},
                    ui.row{gap = 8,
                        ui.column{gap = 4, grow = 1,
                            ui.label{text = 'Invite a friend', font = 'heading'},
                            ui.label{text = 'Your friend gets a free night ride, and so do you.', font = 'caption', color = 'textMuted'},
                            ui.button{id = 'invite', text = 'Share the invite', variant = 'primary', align = 'start', margin = {6, 0, 0, 0}, onClick = function()
                                Shell.notify('Your invite is ready to share.')
                            end},
                        },
                        ui.image{image = 'images/hero.png', width = 180, height = 135, fit = 'contain', accessibilityHidden = true},
                    },
                },
                ui.settingsForm{
                    ui.sectionTitle{text = 'Look'},
                    ui.settingsRow{label = 'Day theme', caption = 'A light map and screens for the daytime.', ui.toggle{id = 'day', checked = ui.theme() == 'rideDay', onChange = function(event)
                        preferences.set('day', event.checked)
                        ui.setTheme(event.checked and 'rideDay' or 'rideNight')
                    end}},
                    ui.sectionTitle{text = 'Safety'},
                    ui.settingsRow{label = 'Share every trip', caption = 'Trusted contacts follow your rides on a map.', ui.toggle{id = 'share', checked = false}},
                    ui.settingsRow{label = 'Quiet rides', caption = 'Ask drivers for a ride without small talk.', ui.toggle{id = 'quiet', checked = true}},
                },
                ui.sectionTitle{text = 'Trusted contacts'},
                ui.card{padding = 6, ui.list{id = 'contacts', items = {{id = 'tom', text = 'Tomas Almeida', caption = 'Brother', image = Shell.icon('user')}, {id = 'ines', text = 'Ines Kato', caption = 'Friend', image = Shell.icon('user')}}}},
                ui.button{id = 'signOut', text = 'Sign out', icon = Shell.icon('close'), variant = 'destructive', align = 'start', onClick = function()
                    Shell.notify('This sample keeps you signed in.', 'information')
                end},
            },
        },
    }))
end

return Account
