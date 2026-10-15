-- The queue: the song that plays and the songs that come next, which the person drags into another order with the pointer or carries with the keyboard, a gamepad or a remote, and removes from their menu.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Player = require('player')
local Shell = require('shell')

local Queue = haylen.class('Queue', Page)

function Queue:enter()
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Queue', {
            ui.button{id = 'clear', text = 'Clear', variant = 'toolbar', onClick = function()
                for index = #Player.queue, Player.index + 1, -1 do
                    table.remove(Player.queue, index)
                end
                Player.changed()
            end},
        }),
        Shell.centered(900, ui.column{gap = 8, padding = {0, 12, 0, 12},
            ui.label{text = 'Now playing', font = 'caption', color = 'textMuted', margin = {0, 8}},
            ui.row{gap = 14, padding = {6, 8},
                ui.image{id = 'nowCover', image = Shell.blankCover, width = 56, height = 56, radius = 8},
                ui.column{gap = 0, grow = 1, ui.label{id = 'nowName', text = '', font = 'button', color = 'accentText', wrap = false}, ui.label{id = 'nowArtist', text = '', font = 'caption', color = 'textMuted', wrap = false}},
            },
            ui.label{id = 'nextLabel', text = 'Next up', font = 'caption', color = 'textMuted', margin = {8, 8, 0, 8}},
        }),
        Shell.centered(900, ui.collection{id = 'upcoming', gap = 2, padding = {0, 12, 24, 12}, reorderable = true, rememberFocus = true,
            types = {entry = {template = ui.contextMenu{part = 'menu', items = {{id = 'remove', text = 'Remove from the queue', image = Shell.icon('trash')}, {id = 'next', text = 'Play now', image = Shell.icon('play')}},
                ui.row{gap = 14, padding = {6, 8},
                    ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = 52, height = 52, radius = 8},
                    ui.column{gap = 0, grow = 1,
                        ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
                        ui.label{part = 'artist', bind = {text = 'artist'}, font = 'caption', color = 'textMuted', wrap = false},
                    },
                    ui.label{part = 'duration', bind = {text = 'duration'}, font = 'caption', color = 'textMuted'},
                    ui.icon{image = Shell.icon('more'), size = 24, color = 'textMuted'},
                },
            }, interactive = true, estimatedSize = 66}},
            onReorder = function(event)
                Player.moveInQueue(Player.index + event.from, Player.index + event.to)
            end,
            onSelect = function(event)
                if event.part == 'menu' then
                    local position = Player.index + event.cell.index
                    if event.item == 'remove' then
                        Player.removeFromQueue(position)
                    else
                        Player.index = position
                        Player.load()
                    end
                end
            end,
            ui.emptyState{image = Shell.icon('queue'), imageSize = 72, title = 'Nothing comes next', message = 'Add songs to the queue from their menu.'},
        }, 1),
    })
    self.list = self.gui:collection('upcoming')
    events.on('playerChanged', function()
        self:showQueue()
    end, {owner = self})
    self:showQueue()
end

-- The entries of the songs after the one that plays, with ids from their place, since a song may come twice.
function Queue:showQueue()
    local track = Player.current()
    self.gui:set('nowCover', {image = track and track.cover or Shell.blankCover})
    self.gui:set('nowName', {text = track and track.name or 'Nothing plays'})
    self.gui:set('nowArtist', {text = track and track.artistName or ''})
    local entries = {}
    for index = Player.index + 1, #Player.queue do
        local upcoming = Player.queue[index]
        entries[#entries + 1] = {id = 'entry-' .. index .. '-' .. upcoming.id, type = 'entry', name = upcoming.name, artist = upcoming.artistName, cover = upcoming.cover, duration = upcoming.durationText}
    end
    self.list:setItems(entries)
    self.gui:set('nextLabel', {text = #entries == 1 and 'Next up, 1 song' or string.format('Next up, %d songs', #entries)})
end

return Queue
