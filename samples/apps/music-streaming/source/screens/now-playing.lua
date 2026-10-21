-- The now playing screen: the big cover over the glow of the colors of its genre, a visualizer whose bars follow the voices of the song as the synthesizer plays them, the song with its heart, the position that the person drags to seek, the controls with shuffle and repeat, the sleep timer, the volume and the way to the queue. A wide window shows the cover beside the controls and the songs that come next.
local audio = require('haylen.audio')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local math2d = require('haylen.math')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Catalog = require('data.catalog')
local Covers = require('covers')
local Page = require('page')
local Player = require('player')
local Shell = require('shell')
local Style = require('style')
local Synth = require('synth')
local Widgets = require('widgets')

local NowPlaying = haylen.class('NowPlaying', Page)

NowPlaying.backTransition = {duration = 0.3, effect = 'push', direction = 'down', ease = 'cubicOut'}
NowPlaying.sleepItems = {{id = 'off', text = 'Off'}, {id = '5', text = 'In 5 minutes'}, {id = '15', text = 'In 15 minutes'}, {id = '30', text = 'In 30 minutes'}, {id = 'track', text = 'At the end of the song'}}
-- The bars of the visualizer, and how fast they rise to a level and fall back from it.
NowPlaying.bars = 36
NowPlaying.rise = 28
NowPlaying.fall = 6

function NowPlaying:enter()
    self.levels = {}
    self.heights = {}
    for index = 1, NowPlaying.bars do
        self.heights[index] = 0
    end
    self:mountGui(ui.column{id = 'root', gap = 0, onCancel = Shell.back})
    events.on('playerChanged', function()
        self:showSong()
    end, {owner = self})
end

function NowPlaying:layoutChanged(wide)
    local space = Style.space
    local top = ui.row{gap = space.xs, padding = {0, space.s}, height = Style.size.header,
        ui.button{id = 'close', icon = Shell.icon('down'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Close the player', tooltip = 'Close', onClick = Shell.back},
        ui.column{gap = 0, grow = 1, ui.label{text = 'Playing from', font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'}, ui.label{id = 'source', text = '', font = 'button', textAlign = 'center', align = 'stretch', wrap = false}},
        ui.button{id = 'queue', icon = Shell.icon('queue'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Queue', tooltip = 'Queue', onClick = function()
            Shell.open(require('screens.queue'))
        end},
    }
    local art = ui.column{gap = space.m, grow = 1,
        ui.image{id = 'cover', image = Shell.blankCover, aspectRatio = 1, align = 'center', grow = 1, maxWidth = 560, maxHeight = 560, radius = Style.radius.card, fit = 'contain', accessibilityLabel = 'Cover'},
        ui.spacer{id = 'visualizer', height = wide and 120 or 76, align = 'stretch'},
    }
    local controls = ui.column{gap = space.s,
        ui.row{gap = space.s,
            ui.column{gap = 0, grow = 1,
                ui.label{id = 'title', text = '', font = 'title', wrap = false, style = Style.text('title', Style.type.heading * 1.2)},
                ui.button{id = 'artist', text = '', variant = 'link', align = 'start', style = {metrics = {controlPaddingX = 0}, colors = {accentText = 'textMuted', accentHover = 'text'}, fonts = {button = {size = Style.type.body, bold = false}}}, onClick = function()
                    local track = Player.current()
                    if track then
                        Shell.open(require('screens.artist'), {artist = track.artist})
                    end
                end},
            },
            ui.button{id = 'like', icon = Shell.icon('heart'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Like', tooltip = 'Like', onClick = function()
                local track = Player.current()
                if track then
                    Catalog.toggleLike(track)
                end
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
        ui.row{gap = space.s, ui.label{id = 'elapsed', text = '0:00', font = 'caption', color = 'textMuted', grow = 1}, ui.label{id = 'length', text = '0:00', font = 'caption', color = 'textMuted'}},
        ui.row{gap = space.xs, justify = 'spaceBetween', margin = {space.xs, 0},
            ui.button{id = 'shuffle', icon = Shell.icon('shuffle'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Shuffle', tooltip = 'Shuffle', onClick = function()
                Player.setShuffle(not Player.shuffle)
            end},
            ui.button{id = 'previous', icon = Shell.icon('previous'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Previous song', style = {metrics = {iconSize = 40}}, onClick = Player.previous},
            Widgets.playButton('toggle', 92, {accessibilityLabel = 'Play or pause', onClick = Player.toggle}, true),
            ui.button{id = 'next', icon = Shell.icon('next'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Next song', style = {metrics = {iconSize = 40}}, onClick = Player.next},
            ui.button{id = 'repeat', icon = Shell.icon('repeat'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Repeat', tooltip = 'Repeat', onClick = Player.cycleRepeat},
        },
        ui.label{id = 'mode', text = '', font = 'caption', color = 'accentText', textAlign = 'center', align = 'stretch'},
        ui.row{gap = space.s,
            ui.menuButton{id = 'sleep', text = 'Sleep timer', icon = Shell.icon('timer'), iconColor = 'text', variant = 'toolbar', items = NowPlaying.sleepItems, onSelect = function(event)
                self:setSleep(event.item)
            end},
            ui.spacer{grow = 1},
            ui.icon{image = Shell.icon('volume'), size = 26, color = 'textMuted'},
            ui.slider{id = 'volume', value = audio.busVolume('music'), min = 0, max = 1, width = 180, accessibilityLabel = 'Volume', onChange = function(event)
                audio.setBusVolume('music', event.value)
            end},
        },
    }
    local body
    if wide then
        controls.width = 520
        body = ui.row{grow = 1, gap = space.xxl, padding = {space.m, space.xxl, space.xl, space.xxl}, align = 'stretch',
            art,
            ui.column{width = 520, gap = space.l, justify = 'center', controls, ui.label{text = 'Up next', font = 'heading'}, ui.column{id = 'upNext', gap = space.xs}},
        }
    else
        body = Shell.centered(600, ui.column{gap = space.m, padding = {space.xs, space.l, space.l, space.l}, art, controls}, 1)
    end
    self.gui:replaceChildren('root', {top, body})
    self:showSong()
end

function NowPlaying:showSong()
    local track = Player.current()
    local gui = self.gui
    if not track or not gui:has('cover') then
        return
    end
    self.glow = track.genre
    if self.track ~= track then
        self.track = track
        self.plan = Synth.plan(track.seed, Synth.styles[Catalog.genre(track.genre).style])
        self.style = Synth.styles[Catalog.genre(track.genre).style]
    end
    gui:set('cover', {image = track.cover})
    gui:set('source', {text = track.albumName})
    gui:set('title', {text = Player.loading and 'Making the song' or track.name})
    gui:set('artist', {text = track.artistName})
    gui:set('length', {text = track.durationText})
    gui:set('like', {icon = Shell.icon(track.liked and 'heart-filled' or 'heart'), iconColor = track.liked and 'accentText' or 'text'})
    gui:set('toggle', {icon = Shell.icon(Player.playing and 'pause' or 'play')})
    gui:set('shuffle', {iconColor = Player.shuffle and 'accentText' or 'text'})
    gui:set('repeat', {iconColor = Player.repeatMode ~= 'off' and 'accentText' or 'text'})
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
    if gui:has('upNext') then
        local rows = {}
        for index = Player.index + 1, math.min(#Player.queue, Player.index + 4) do
            local upcoming = Player.queue[index]
            rows[#rows + 1] = ui.row{gap = Style.space.s,
                ui.image{image = upcoming.cover, width = 56, height = 56, radius = Style.radius.cover},
                ui.column{gap = 0, grow = 1, ui.label{text = upcoming.name, wrap = false}, ui.label{text = upcoming.artistName, font = 'caption', color = 'textMuted', wrap = false}},
                ui.label{text = upcoming.durationText, font = 'caption', color = 'textMuted'},
            }
        end
        gui:replaceChildren('upNext', rows)
    end
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

-- Moves the position and the bars of the visualizer every frame: each bar rises fast to the level of the voices it stands for and falls back slowly.
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
    local sounding = self.plan and Player.playing and not Player.loading
    local levels = sounding and Synth.levels(self.plan, self.style, elapsed, self.levels) or nil
    local time = haylen.elapsed()
    for index = 1, NowPlaying.bars do
        local target = 0
        if levels then
            local place = (index - 0.5) / NowPlaying.bars
            if place < 0.22 then
                target = levels.kick * 0.9 + levels.bass * 0.55
            elseif place < 0.48 then
                target = levels.bass * 0.45 + levels.pad * 0.55
            elseif place < 0.74 then
                target = levels.lead * 0.85 + levels.pad * 0.3
            else
                target = levels.hats * 0.75 + levels.snare * 0.6
            end
            target = math.min(1, target * (0.82 + 0.18 * math.sin(time * (3 + index % 5) + index * 1.7)))
        end
        local height = self.heights[index]
        local speed = target > height and NowPlaying.rise or NowPlaying.fall
        self.heights[index] = height + (target - height) * math.min(1, dt * speed)
    end
end

-- Draws the glow of the genre, then the bars of the visualizer in the room the GUI leaves for them, from violet at the low end to pink at the high end, with a faint reflection under them.
function NowPlaying:render()
    graphics2d.beginScreen()
    local visible = viewport.visibleRect()
    graphics2d.drawRect(visible, ui.themeColor('window'))
    if self.glow then
        graphics2d.draw(Covers.backdrop(self.glow), visible.x, visible.y, {width = visible.width, height = visible.height, pivotX = 0, pivotY = 0})
    end
    local area = self.gui and self.gui:bounds('visualizer')
    if not area then
        return
    end
    local count = NowPlaying.bars
    local gap = area.width / count * 0.32
    local width = (area.width - gap * (count - 1)) / count
    local base = area.y + area.height * 0.78
    local low, high = math2d.color(Style.color.violet), math2d.color(Style.color.pink)
    for index = 1, count do
        local share = (index - 1) / (count - 1)
        local color = math2d.color(low.r + (high.r - low.r) * share, low.g + (high.g - low.g) * share, low.b + (high.b - low.b) * share, 1)
        local height = math.max(width, self.heights[index] * area.height * 0.76)
        local x = area.x + (index - 1) * (width + gap)
        graphics2d.drawShape({x, base - height, width, height}, {radius = width / 2, color = color})
        graphics2d.drawShape({x, base + gap, width, height * 0.22}, {radius = width / 2, color = math2d.color(color.r, color.g, color.b, 0.22)})
    end
end

return NowPlaying
