-- The now playing screen: the cover, the song with its heart, the position that the person drags to seek, the controls with shuffle and repeat, the sleep timer, the volume and the way to the queue.
local audio = require('haylen.audio')
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Page = require('page')
local Player = require('player')
local Shell = require('shell')

local NowPlaying = haylen.class('NowPlaying', Page)

NowPlaying.backTransition = {duration = 0.3, effect = 'push', direction = 'down', ease = 'cubicOut'}
NowPlaying.sleepItems = {{id = 'off', text = 'Off'}, {id = '5', text = 'In 5 minutes'}, {id = '15', text = 'In 15 minutes'}, {id = '30', text = 'In 30 minutes'}, {id = 'track', text = 'At the end of the song'}}

function NowPlaying:enter()
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        ui.row{wrap = true, gap = 8, padding = {10, 12},
            ui.button{id = 'close', icon = Shell.icon('down'), variant = 'icon', accessibilityLabel = 'Close the player', tooltip = 'Close', onClick = Shell.back},
            ui.label{text = 'Now playing', font = 'button', textAlign = 'center', grow = 1},
            ui.button{id = 'queue', icon = Shell.icon('queue'), variant = 'icon', accessibilityLabel = 'Queue', tooltip = 'Queue', onClick = function()
                Shell.open(require('screens.queue'))
            end},
        },
        ui.scroll{grow = 1, height = 0,
            Shell.centered(560, ui.column{gap = 14, padding = {4, 24, 24, 24},
                ui.image{id = 'cover', image = Shell.blankCover, align = 'stretch', aspectRatio = 1, maxHeight = 520, radius = 22, fit = 'contain'},
                ui.row{wrap = true, gap = 12, margin = {8, 0, 0, 0},
                    ui.column{gap = 0, grow = 1,
                        ui.label{id = 'title', text = '', font = 'heading', wrap = false},
                        ui.button{id = 'artist', text = '', variant = 'link', align = 'start', style = {metrics = {controlPaddingX = 0}}, onClick = function()
                            local track = Player.current()
                            if track then
                                Shell.open(require('screens.artist'), {artist = track.artist})
                            end
                        end},
                    },
                    ui.button{id = 'like', icon = Shell.icon('heart'), iconColor = 'accentText', variant = 'icon', accessibilityLabel = 'Like', tooltip = 'Like', onClick = function()
                        self:toggleLike()
                    end},
                },
                ui.slider{id = 'position', value = 0, min = 0, max = 1, accessibilityLabel = 'Position in the song', onPress = function()
                    self.scrubbing = true
                end, onRelease = function()
                    self.scrubbing = false
                end, onChange = function(event)
                    local track = Player.current()
                    if track then
                        Player.seek(event.value * track.length)
                    end
                end},
                ui.row{gap = 8, ui.label{id = 'elapsed', text = '0:00', font = 'caption', color = 'textMuted', grow = 1}, ui.label{id = 'length', text = '0:00', font = 'caption', color = 'textMuted'}},
                ui.row{wrap = true, gap = 6, justify = 'center',
                    ui.button{id = 'shuffle', icon = Shell.icon('shuffle'), variant = 'toolbar', accessibilityLabel = 'Shuffle', tooltip = 'Shuffle', onClick = function()
                        Player.setShuffle(not Player.shuffle)
                    end},
                    ui.button{id = 'previous', icon = Shell.icon('previous'), variant = 'icon', accessibilityLabel = 'Previous song', onClick = Player.previous},
                    ui.button{id = 'toggle', icon = Shell.icon('pause'), variant = 'primary', width = 84, height = 84, accessibilityLabel = 'Play or pause', style = {metrics = {controlRadius = 42, iconSize = 36, controlPaddingX = 0}}, onClick = Player.toggle},
                    ui.button{id = 'next', icon = Shell.icon('next'), variant = 'icon', accessibilityLabel = 'Next song', onClick = Player.next},
                    ui.button{id = 'repeat', icon = Shell.icon('repeat'), variant = 'toolbar', accessibilityLabel = 'Repeat', tooltip = 'Repeat', onClick = Player.cycleRepeat},
                },
                ui.label{id = 'mode', text = '', font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'},
                ui.row{gap = 12,
                    ui.menuButton{id = 'sleep', text = 'Sleep timer', icon = Shell.icon('timer'), variant = 'toolbar', items = NowPlaying.sleepItems, onSelect = function(event)
                        self:setSleep(event.item)
                    end},
                    ui.icon{image = Shell.icon('volume'), size = 26, color = 'textMuted'},
                    ui.slider{id = 'volume', value = audio.busVolume('music'), min = 0, max = 1, grow = 1, accessibilityLabel = 'Volume', onChange = function(event)
                        audio.setBusVolume('music', event.value)
                    end},
                },
            }),
        },
    })
    events.on('playerChanged', function()
        self:showSong()
    end, {owner = self})
    self:showSong()
end

function NowPlaying:showSong()
    local track = Player.current()
    local gui = self.gui
    if not track then
        return
    end
    gui:set('cover', {image = track.cover})
    gui:set('title', {text = Player.loading and 'Making the song' or track.name})
    gui:set('artist', {text = track.artistName})
    gui:set('length', {text = track.durationText})
    gui:set('like', {icon = Shell.icon(track.liked and 'heart-filled' or 'heart')})
    gui:set('toggle', {icon = Shell.icon(Player.playing and 'pause' or 'play')})
    gui:set('shuffle', {checked = Player.shuffle})
    gui:set('repeat', {checked = Player.repeatMode ~= 'off'})
    local modes = {}
    if Player.shuffle then
        modes[#modes + 1] = 'Shuffle on'
    end
    if Player.repeatMode ~= 'off' then
        modes[#modes + 1] = Player.repeatMode == 'one' and 'Repeating this song' or 'Repeating the queue'
    end
    if Player.sleepLeft then
        modes[#modes + 1] = Player.sleepLeft == 'track' and 'Sleeps after this song' or 'Sleep timer on'
    end
    gui:set('mode', {text = table.concat(modes, ' · ')})
end

function NowPlaying:toggleLike()
    local track = Player.current()
    if not track then
        return
    end
    track.liked = not track.liked
    if track.liked then
        table.insert(Catalog.liked, 1, track)
    else
        for index, other in ipairs(Catalog.liked) do
            if other == track then
                table.remove(Catalog.liked, index)
                break
            end
        end
    end
    Shell.notify(track.liked and 'Added to your liked songs.' or 'Removed from your liked songs.')
    self:showSong()
end

function NowPlaying:setSleep(item)
    if item == 'off' then
        Player.setSleep(nil)
    elseif item == 'track' then
        Player.setSleep('track')
    else
        Player.setSleep(tonumber(item) * 60)
    end
    Shell.notify(item == 'off' and 'The sleep timer is off.' or 'The sleep timer is set.')
end

function NowPlaying:update(dt)
    Page.update(self, dt)
    local track = Player.current()
    if not track then
        return
    end
    local elapsed = Player.elapsed()
    local seconds = math.floor(elapsed)
    if self.shownSecond ~= seconds then
        self.shownSecond = seconds
        self.gui:set('elapsed', {text = Catalog.duration(seconds)})
    end
    if not self.scrubbing then
        local value = math.floor(elapsed / track.length * 500) / 500
        if self.shownValue ~= value then
            self.shownValue = value
            self.gui:set('position', {value = value})
        end
    end
end

return NowPlaying
