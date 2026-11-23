-- The now playing screen, which the app presents over everything as a flow of its own: the big cover over the glow of the colors of its genre, a visualizer whose bars follow the voices of the song as the synthesizer plays them, the song with its heart, the position that the person drags to seek, the controls with shuffle and repeat, the sleep timer, the volume, the way to the queue and the share button of the song. A wide window shows the cover beside the controls and the songs that come next.
local audio = require('haylen.audio')
local events = require('haylen.events')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local math2d = require('haylen.math')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Catalog = require('data.catalog')
local Covers = require('covers')
local Page = require('page')
local Player = require('player')
local Routes = require('routes')
local Shell = require('shell')
local Style = require('style')
local Synth = require('synth')
local Widgets = require('widgets')

local NowPlaying = haylen.class('NowPlaying', Page)

NowPlaying.sleepItems = {{id = 'off', text = 'Off'}, {id = '5', text = 'In 5 minutes'}, {id = '15', text = 'In 15 minutes'}, {id = '30', text = 'In 30 minutes'}, {id = 'track', text = 'At the end of the song'}}
-- The bars of the visualizer, and how fast they rise to a level and fall back from it.
NowPlaying.bars = 36
NowPlaying.rise = 28
NowPlaying.fall = 6

-- A bar lower than this share of its height rests, so the visualizer asks for no more frames once the song stops.
NowPlaying.rest = 0.002

NowPlaying.clear = {clear = '#00000000'}

function NowPlaying:init(params)
    Page.init(self, params)
    self.levels = {}
    self.heights = {}
    for index = 1, NowPlaying.bars do
        self.heights[index] = 0
    end
    local track = Player.current()
    self.glow = track and track.genre
    self.camera = graphics2d.newCamera()
    self.camera.anchor = 'topLeft'
    self.low, self.high = math2d.color(Style.color.violet), math2d.color(Style.color.pink)
    self.colors = {}
    for index = 1, NowPlaying.bars do
        self.colors[index] = math2d.color(0, 0, 0, 1)
    end
end

function NowPlaying:tree()
    return ui.column{id = 'root', gap = 0}
end

function NowPlaying:start()
    events.on('playerChanged', function()
        self:showSong()
    end, {owner = self})
end

-- The glow of the player reaches lower than the glow of a page.
function NowPlaying:build()
    local tree = Page.build(self)
    tree[1].image = self.glow and Covers.glow(self.glow, Covers.playerReach) or Shell.blankCover
    return tree
end

function NowPlaying:layoutChanged(wide)
    local space = Style.space
    local top = ui.row{gap = space.xs, padding = {0, space.s}, height = Style.size.header,
        ui.button{id = 'close', icon = Shell.icon('down'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Close the player', tooltip = 'Close', onClick = function()
            self.presentation:dismiss()
        end},
        ui.column{gap = 0, grow = 1, ui.label{text = 'Playing from', font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'}, ui.label{id = 'source', text = '', font = 'button', textAlign = 'center', align = 'stretch', wrap = false}},
        ui.button{id = 'share', icon = Shell.icon('share'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Share the song', tooltip = 'Share', onClick = function()
            self:share()
        end},
        ui.button{id = 'queue', icon = Shell.icon('queue'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Queue', tooltip = 'Queue', onClick = Shell.openQueue},
    }
    local art = ui.column{gap = space.m, grow = 1,
        ui.image{id = 'cover', image = Shell.blankCover, aspectRatio = 1, align = 'center', grow = 1, maxWidth = 560, maxHeight = 560, radius = Style.radius.card, fit = 'contain', accessibilityLabel = 'Cover'},
        ui.stack{id = 'visualizerBox', height = wide and 120 or 76, align = 'stretch',
            ui.image{id = 'visualizer', image = Shell.blankCover, visible = false, fit = 'fill', align = 'stretch', accessibilityHidden = true},
        },
    }
    local controls = ui.column{gap = space.s,
        ui.row{gap = space.s,
            ui.column{gap = 0, grow = 1,
                ui.label{id = 'title', text = '', font = 'title', wrap = false, style = Style.text('title', Style.type.heading * 1.2)},
                ui.button{id = 'artist', text = '', variant = 'link', align = 'start', style = {metrics = {controlPaddingX = 0}, colors = {accentText = 'textMuted', accentHover = 'text'}, fonts = {button = {size = Style.type.body, bold = false}}}, onClick = function()
                    local track = Player.current()
                    if track then
                        self.presentation:dismiss()
                        Shell.open(Routes.location(track.artist))
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
    if not gui:has('cover') then
        return
    end
    if not track then
        gui:set('title', {text = 'Nothing plays'})
        gui:set('artist', {text = 'Pick a song to start'})
        return
    end
    if self.glow ~= track.genre then
        self.glow = track.genre
        gui:set('glow', {image = Covers.glow(track.genre, Covers.playerReach), visible = true})
    end
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

-- Offers the links of the song in an action sheet that points at the share button.
function NowPlaying:share()
    local track = Player.current()
    if track then
        self.shareSheet = Shell.share(track.name, Routes.location(track.id), {gui = self.gui, id = 'share'})
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
        if self.heights[index] > NowPlaying.rest then
            window.requestRedraw()
        end
    end
end

-- Draws the bars of the visualizer into the picture that the GUI shows where it leaves room for them, from violet at the low end to pink at the high end, with a faint reflection under them. The picture takes the pixels it covers on the screen, so the bars stay sharp and move with the screen as it rises and leaves.
function NowPlaying:render()
    local area = self.gui:bounds('visualizerBox')
    if not area or area.width <= 0 then
        return
    end
    local density = viewport.pixelsPerUnit()
    local width, height = math.floor(area.width * density + 0.5), math.floor(area.height * density + 0.5)
    if not self.target or math.abs(width - self.targetWidth) > 1 or math.abs(height - self.targetHeight) > 1 then
        self.target = graphics.newRenderTarget(width, height)
        self.targetWidth, self.targetHeight = width, height
        self.camera.x, self.camera.y = 0, 0
        if self.visualizer then
            ui.removeImage(self.visualizer)
        end
        self.visualizer = '@visualizer-' .. tostring(self.target)
        ui.setImage(self.visualizer, self.target)
        self.gui:set('visualizer', {image = self.visualizer, visible = true})
    end
    graphics2d.beginTarget(self.target, self.camera, NowPlaying.clear)
    local count = NowPlaying.bars
    local gap = width / count * 0.32
    local barWidth = (width - gap * (count - 1)) / count
    local base = height * 0.78
    local low, high = self.low, self.high
    for index = 1, count do
        local share = (index - 1) / (count - 1)
        local color = self.colors[index]
        color.r, color.g, color.b, color.a = low.r + (high.r - low.r) * share, low.g + (high.g - low.g) * share, low.b + (high.b - low.b) * share, 1
        local barHeight = math.max(barWidth, self.heights[index] * height * 0.76)
        local x = (index - 1) * (barWidth + gap)
        graphics2d.drawShape({x, base - barHeight, barWidth, barHeight}, {radius = barWidth / 2, color = color})
        color.a = 0.22
        graphics2d.drawShape({x, base + gap, barWidth, barHeight * 0.22}, {radius = barWidth / 2, color = color})
    end
end

function NowPlaying:remove()
    if self.visualizer then
        ui.removeImage(self.visualizer)
    end
end

return NowPlaying
