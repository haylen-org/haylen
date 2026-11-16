-- The queue: the song that plays and the songs that come next, which the person drags into another order with the pointer or carries with the keyboard, a gamepad or a remote, and removes from their menu or clears, with a snackbar that brings them back.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Player = require('player')
local Shell = require('shell')
local Style = require('style')

local Queue = haylen.class('Queue', Page)

-- The queue is a sheet over the app or the player, on the surface of the sheet, which rests at half the screen, opens to nearly all of it with a drag or a tap on its grabber, and closes with its button, a drag down or the system back.
function Queue:tree()
    local space = Style.space
    return ui.column{grow = 1, gap = 0,
        Shell.sheetHeader('Queue', {
            ui.button{id = 'clear', text = 'Clear', variant = 'toolbar', onClick = function()
                self:clear()
            end},
        }),
        Shell.centered(900, ui.column{gap = space.xs, padding = {space.s, space.m, 0, space.m},
            ui.label{text = 'Now playing', font = 'caption', color = 'textMuted', margin = {0, space.s}},
            ui.row{gap = space.m, padding = {space.xs, space.s},
                ui.image{id = 'nowCover', image = Shell.blankCover, width = 72, height = 72, radius = Style.radius.cover},
                ui.column{gap = 0, grow = 1, ui.label{id = 'nowName', text = '', font = 'button', color = 'accentText', wrap = false}, ui.label{id = 'nowArtist', text = '', font = 'caption', color = 'textMuted', wrap = false}},
            },
            ui.label{id = 'nextLabel', text = 'Next up', font = 'heading', margin = {space.m, space.s, 0, space.s}},
        }),
        Shell.centered(900, ui.collection{id = 'upcoming', gap = 0, padding = {0, space.m, space.xl, space.m}, reorderable = true, rememberFocus = true,
            types = {entry = {template = ui.contextMenu{part = 'menu', items = {{id = 'remove', text = 'Remove from the queue', image = Shell.icon('trash')}, {id = 'next', text = 'Play now', image = Shell.icon('play')}},
                ui.row{gap = space.m, padding = {space.xs, space.s},
                    ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = 60, height = 60, radius = Style.radius.cover},
                    ui.column{gap = 0, grow = 1,
                        ui.label{part = 'name', bind = {text = 'name'}, wrap = false},
                        ui.label{part = 'artist', bind = {text = 'artist'}, font = 'caption', color = 'textMuted', wrap = false},
                    },
                    ui.label{part = 'duration', bind = {text = 'duration'}, font = 'caption', color = 'textMuted'},
                    ui.icon{image = Shell.icon('more'), size = 26, color = 'textMuted'},
                },
            }, interactive = true, estimatedSize = 84}},
            onReorder = function(event)
                Player.moveInQueue(Player.index + event.from, Player.index + event.to)
            end,
            onSelect = function(event)
                if event.part == 'menu' then
                    local position = Player.index + event.cell.index
                    if event.item == 'remove' then
                        self:drop(position)
                    else
                        Player.index = position
                        Player.load()
                    end
                end
            end,
            ui.emptyState{image = Shell.icon('queue'), imageSize = 72, title = 'Nothing comes next', message = 'Add songs to the queue from their menu.'},
        }, 1),
    }
end

function Queue:start()
    self.list = self.gui:collection('upcoming')
    events.on('playerChanged', function()
        self:showQueue()
    end, {owner = self})
    self:showQueue()
end

-- Takes a song out of the queue, which a snackbar offers to bring back to its place.
function Queue:drop(position)
    local track = Player.queue[position]
    Player.removeFromQueue(position)
    Shell.offerUndo(track.name .. ' left the queue.', function()
        Player.putBack({track}, position)
    end)
end

-- Takes the songs that come next out of the queue, which a snackbar offers to bring back.
function Queue:clear()
    local removed = Player.clearUpcoming()
    if #removed == 0 then
        return
    end
    Shell.offerUndo(#removed == 1 and 'One song left the queue.' or string.format('%d songs left the queue.', #removed), function()
        Player.putBack(removed, Player.index + 1)
    end)
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
