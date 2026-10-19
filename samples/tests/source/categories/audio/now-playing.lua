-- Now playing: a streamed track that the now-playing controls of the system show with its title, artist, artwork, length and position, whose play, pause, toggle, stop, next, previous and seek commands, from the controls, the media keys or the buttons of headsets and remotes, change what plays, with a log of the commands as they arrive.
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local events = require('haylen.events')
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

function NowPlaying:enter()
    self.journal = Journal(16)
    self.index = 1
    self.artwork = assets.bytes(NowPlaying.artwork)
    self:frame{
        hint = 'Use the lock screen, the control center, the media notification, the media flyout or the media keys of the keyboard to play, pause, skip and seek.',
        focus = 'start',
        controls = {
            ui.button{id = 'start', text = 'Play and show it in the controls', variant = 'primary', onClick = function() self:start() end},
            ui.button{id = 'clear', text = 'Leave the controls', onClick = function() self:leave() end},
            ui.label{text = 'iOS shows the lock screen only for an audio session that plays, which this test project leaves as ambient. Linux has no controls the engine reaches.', color = 'textMuted', font = 'caption'},
        },
        code = "system.setNowPlaying({title = 'Calm loop', artist = 'Haylen', duration = 32, position = audio.cursor(voice), playing = true, artwork = assets.bytes('audio/cover.png')})\nevents.on('mediaCommand', function(event) print(event.command, event.position) end)",
    }
    events.on('mediaCommand', function(event) self:command(event) end, {owner = self})
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
    self.journal:add(string.format('Command "%s"%s', event.command, event.position and string.format(' at %.1f seconds', event.position) or ''), Test.green)
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
    self:status(string.format('Track "%s", %s, at %.1f seconds', sounds.tracks[self.index].text, state, self.voice and audio.cursor(self.voice) or 0))
end

function NowPlaying:draw(area)
    Test.caption('Commands from the system', 24, 20, {size = 24, color = Test.ink})
    self.journal:draw(24, 64, area.height - 88, 20)
end

return NowPlaying
