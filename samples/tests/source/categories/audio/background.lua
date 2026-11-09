-- Background playback: a playlist of streamed tracks that plays one after another while the app is in the background, as a music player does, with the now-playing controls of the system and a log of each time the app left and came back with how far the music went meanwhile.
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local EventLog = require('categories.audio.event-log')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local Background = haylen.class('Background', AudioTest)

Background.length = 32
Background.artwork = 'audio/cover.png'
Background.code = [[
-- The section "audio" of "app.json" holds {"iosSession": "playback", "background": true}.
haylen.setLifecycle({pauseOnBackground = false})
audio.setBackground(true)
local voice = audio.playMusic(track, {loop = false})
system.setNowPlaying({title = 'Calm loop', duration = 32, playing = true})]]

-- What each platform needs to play on in the background, which the test project has.
Background.notes = {
    ios = 'iOS plays on with the session "playback" and the background mode "audio", which the tools add from "app.json".',
    android = 'Android plays on while the media notification shows the track as playing, which keeps the app in the foreground.',
    web = 'A hidden tab plays on, except in Safari on iPhones and iPads, which suspends the page.',
}

function Background:enter()
    self.saved = {lifecycle = haylen.lifecycle(), background = audio.background()}
    haylen.setLifecycle({pauseOnBackground = false})
    audio.setBackground(true)
    self.clock = 0
    self.played = 0
    self.index = 1
    self.artwork = assets.bytes(Background.artwork)
    self.cover = assets.texture(Background.artwork, {filter = 'linear', mipmaps = true})
    self.log = EventLog({
        title = 'Away and back',
        hint = 'Next: leave the app with the music playing, wait, and come back. The music keeps playing, and the next track starts when one ends.',
        legend = {{Test.green, 'Played on'}, {Test.warm, 'From the app'}, {Test.accent, 'From the system'}},
    })
    self:listen('appBackground', function() self:leave() end)
    self:listen('appActive', function() self:comeBack() end)
    self:listen('mediaCommand', function(event) self:command(event) end)
    self:frame{
        hint = 'Leave the app or lock the device while the music plays, and use the lock screen, the media notification or the media keys.',
        focus = 'toggle',
        controls = {
            ui.button{id = 'toggle', text = 'Play or pause', variant = 'primary', onClick = function() self:toggle() end},
            ui.button{id = 'next', text = 'Next track', onClick = function() self:play(self.index % #sounds.tracks + 1) end},
            ui.label{text = Background.notes[haylen.platform] or 'A desktop app plays on while its window is minimized.', color = 'textMuted', font = 'caption'},
        },
        code = Background.code,
    }
    self:play(1)
end

function Background:exit()
    system.clearNowPlaying()
    audio.stopMusic(0.3)
    audio.setBackground(self.saved.background)
    haylen.setLifecycle(self.saved.lifecycle)
    Background.super.exit(self)
end

-- Plays a track of the playlist once from its start, so its end moves the playlist on.
function Background:play(index)
    self.index = index
    self.voice = audio.playMusic(sounds.track(sounds.tracks[index].path), {fade = 0.5, loop = false})
    self.cursor = 0
    self:show(true)
    self.log:add(self.clock, string.format('The app played the %s.', sounds.tracks[index].text:lower()), Test.warm)
end

function Background:show(playing)
    local track = sounds.tracks[self.index]
    system.setNowPlaying({title = track.text, artist = 'Haylen tests', album = 'Background playback', duration = Background.length, position = audio.cursor(self.voice), playing = playing, artwork = self.artwork})
end

function Background:toggle()
    if audio.paused(self.voice) then
        audio.resume(self.voice)
    else
        audio.pause(self.voice)
    end
    self:show(not audio.paused(self.voice))
end

-- A play, a pause and a toggle change what plays only when it is not already so.
function Background:command(event)
    self.log:add(self.clock, string.format('The command "%s" arrived in the app state "%s".', event.command, haylen.appState()), Test.accent)
    if event.command == 'next' then
        self:play(self.index % #sounds.tracks + 1)
        return
    end
    if event.command == 'previous' then
        self:play((self.index - 2) % #sounds.tracks + 1)
        return
    end
    if event.command == 'seek' then
        audio.setCursor(self.voice, event.position)
        self.cursor = event.position
        self:show(not audio.paused(self.voice))
        return
    end

    local paused = audio.paused(self.voice)
    local plays = event.command == 'play' or (event.command == 'togglePlayPause' and paused)
    if plays == paused then
        self:toggle()
    end
end

function Background:leave()
    self.away = {clock = self.clock, played = self.played}
    self.log:add(self.clock, string.format('The app went to the background at %s of the %s.', AudioTest.clock(audio.cursor(self.voice)), sounds.tracks[self.index].text:lower()), Test.warm)
end

function Background:comeBack()
    if not self.away then
        return
    end
    local played = self.played - self.away.played
    self.log:add(self.clock, string.format('The app came back, and the music played %s while it was away, now at %s of the %s.', AudioTest.clock(played), AudioTest.clock(audio.cursor(self.voice)), sounds.tracks[self.index].text:lower()), Test.green)
    self.away = nil
end

-- The music counts what it played by the cursor of its voice, so the count holds whatever pace the frames had in the background.
function Background:update(dt)
    Background.super.update(self, dt)
    self.clock = self.clock + haylen.unscaledDelta()
    if not audio.active(self.voice) then
        self:play(self.index % #sounds.tracks + 1)
        return
    end
    local cursor = audio.cursor(self.voice)
    if cursor > self.cursor then
        self.played = self.played + cursor - self.cursor
    end
    self.cursor = cursor
    local state = audio.paused(self.voice) and 'is paused' or 'plays'
    self:status(string.format('The %s %s at %s of %s in the app state "%s", and the sound %s in the background.', sounds.tracks[self.index].text:lower(), state, AudioTest.clock(cursor), AudioTest.clock(Background.length), haylen.appState(), audio.background() and 'plays on' or 'stops'))
end

-- The card of what plays beside the log of the times the app was away.
function Background:draw(area)
    local margin = 24
    local size = math.max(120, math.min(area.height - margin * 2 - 150, area.width * 0.28))
    local cardWidth = size + 48
    graphics2d.drawShape({margin, margin, cardWidth, area.height - margin * 2}, {radius = 22, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    local left, top = margin + 24, margin + 24
    graphics2d.drawShape({left, top, size, size}, {radius = 16, texture = self.cover, layer = 1})

    local playing = not audio.paused(self.voice)
    local cursor = audio.cursor(self.voice)
    local y = top + size + 14
    Test.caption(sounds.tracks[self.index].text, left, y, {size = 28, color = Test.ink, maxWidth = size, layer = 1})
    Test.caption(string.format('Track %d of %d, %s', self.index, #sounds.tracks, playing and 'playing' or 'paused'), left, y + 38, {size = 20, maxWidth = size, wrap = false, minScale = 0.8, layer = 1})
    AudioTest.bar(left, y + 74, size, 10, cursor / Background.length, playing and Test.accent or Test.muted)

    local logLeft = margin * 2 + cardWidth
    if area.width - logLeft > 200 then
        self.log:draw(logLeft, margin, area.width - logLeft - margin, area.height - margin * 2)
    end
    AudioTest.lockNote(area.width - margin - 24, area.height - margin - 140)
end

return Background
