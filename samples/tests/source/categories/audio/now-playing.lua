-- Now playing: a streamed track that the now-playing controls of the system show with its title, artist, artwork, length and position, whose play, pause, toggle, stop, next, previous and seek commands, from the controls, the media keys or the buttons of headsets and remotes, change what plays, with a log of the commands as they arrive.
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local Journal = require('harness.journal')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local NowPlaying = haylen.class('NowPlaying', AudioTest)

NowPlaying.length = 32
NowPlaying.artwork = 'audio/cover.png'
NowPlaying.code = [[
system.setNowPlaying({
    title = 'Calm loop', artist = 'Haylen tests', duration = 32, position = audio.cursor(voice), playing = true,
    artwork = assets.bytes('audio/cover.png'),
})
events.on('mediaCommand', function(event) print(event.command, event.position) end, {owner = self})]]

function NowPlaying:enter()
    self.journal = Journal(16)
    self.index = 1
    self.artwork = assets.bytes(NowPlaying.artwork)
    self.cover = assets.texture(NowPlaying.artwork, {filter = 'linear', mipmaps = true})
    self:frame{
        hint = 'Use the lock screen, the control center, the media notification, the media flyout or the media keys of the keyboard to play, pause, skip and seek.',
        focus = 'start',
        controls = {
            ui.button{id = 'start', text = 'Play and show it in the controls', variant = 'primary', onClick = function() self:start() end},
            ui.button{id = 'clear', text = 'Leave the controls', onClick = function() self:leave() end},
            ui.label{text = 'iOS shows the lock screen only for an audio session that plays, which this test project leaves as ambient. Linux has no controls the engine reaches.', color = 'textMuted', font = 'caption'},
        },
        code = NowPlaying.code,
    }
    events.on('mediaCommand', function(event) self:command(event) end, {owner = self})
    -- The calm track starts as the test opens, so the controls of the system show it at once.
    self:start()
end

function NowPlaying:exit()
    system.clearNowPlaying()
    audio.stopMusic(0.3)
    NowPlaying.super.exit(self)
end

function NowPlaying:start()
    local track = sounds.tracks[self.index]
    self.voice = audio.playMusic(assets.load(track.path, 'sound', sounds.stream), {fade = 0.5})
    self:show(true)
end

function NowPlaying:leave()
    system.clearNowPlaying()
    self.journal:add('The app left the controls.', Test.warm)
end

function NowPlaying:show(playing)
    local track = sounds.tracks[self.index]
    system.setNowPlaying({title = track.text, artist = 'Haylen tests', album = 'Synthesized loops', duration = NowPlaying.length, position = self.voice and audio.cursor(self.voice) % NowPlaying.length or 0, playing = playing, artwork = self.artwork})
end

-- Every command changes what plays and shows the new state in the controls.
function NowPlaying:command(event)
    self.journal:add(string.format('The command "%s"%s arrived.', event.command, event.position and string.format(' at %.1f seconds', event.position) or ''), Test.green)
    if not self.voice then
        self:start()
        return
    end
    local paused = audio.paused(self.voice)
    if event.command == 'play' or (event.command == 'togglePlayPause' and paused) then
        audio.resume(self.voice)
    elseif event.command == 'pause' or event.command == 'togglePlayPause' then
        audio.pause(self.voice)
    elseif event.command == 'stop' then
        audio.stopMusic(0.3)
        self.voice = nil
        system.clearNowPlaying()
        return
    elseif event.command == 'next' or event.command == 'previous' then
        self.index = self.index % #sounds.tracks + 1
        self:start()
        return
    elseif event.command == 'seek' then
        audio.setCursor(self.voice, event.position)
    end
    self:show(not audio.paused(self.voice))
end

function NowPlaying:update(dt)
    NowPlaying.super.update(self, dt)
    local state = self.voice and (audio.paused(self.voice) and 'paused' or 'playing') or 'stopped'
    self:status(string.format('The track "%s" is %s at %.1f seconds', sounds.tracks[self.index].text, state, self.voice and audio.cursor(self.voice) or 0))
end

-- The card of what plays: the artwork, the title, the artist, the state and a bar of the position, beside the commands as they arrive.
function NowPlaying:draw(area)
    local margin = 24
    local size = math.max(120, math.min(area.height - margin * 2 - 180, area.width * 0.32))
    local cardWidth = math.min(area.width - margin * 2, size + 48)
    graphics2d.drawShape({margin, margin, cardWidth, area.height - margin * 2}, {radius = 22, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    local left, top = margin + 24, margin + 24
    graphics2d.drawShape({left, top, size, size}, {radius = 16, texture = self.cover, layer = 1})

    local track = sounds.tracks[self.index]
    local playing = self.voice ~= nil and not audio.paused(self.voice)
    local position = self.voice and audio.cursor(self.voice) % NowPlaying.length or 0
    local y = top + size + 14
    Test.caption(track.text, left, y, {size = 28, color = Test.ink, maxWidth = size, layer = 1})
    Test.caption(self.voice and (playing and 'Haylen tests, playing' or 'Haylen tests, paused') or 'Haylen tests, stopped', left, y + 38, {size = 20, layer = 1})
    AudioTest.bar(left, y + 74, size, 10, position / NowPlaying.length, playing and Test.accent or Test.muted)
    Test.caption(string.format('%d:%02d', math.floor(position / 60), math.floor(position % 60)), left, y + 92, {size = 18, layer = 1})
    Test.caption(string.format('%d:%02d', NowPlaying.length // 60, NowPlaying.length % 60), left + size, y + 92, {size = 18, anchor = {1, 0}, layer = 1})

    local logLeft = margin * 2 + cardWidth
    if area.width - logLeft > 200 then
        Test.caption('Commands from the system', logLeft, margin + 4, {size = 26, color = Test.ink})
        if self.journal.lines.size == 0 then
            Test.caption('No command yet. Play, pause, skip or seek from the controls of the system or the media keys.', logLeft, margin + 52, {size = 21, maxWidth = area.width - logLeft - margin})
        end
        self.journal:draw(logLeft, margin + 52, area.height - margin * 2 - 52, 21)
    end
end

return NowPlaying
