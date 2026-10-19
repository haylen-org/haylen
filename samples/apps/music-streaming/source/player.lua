-- The player: the queue, the song that plays with its voice of haylen.audio, shuffle, repeat, seeking, the sleep timer and the now playing controls of the system. It is an autoload, so music goes on across every screen. Songs are made by the synthesizer in jobs the first time they play and kept for a while, and every change emits "playerChanged" on the event bus.
local async = require('async')
local audio = require('haylen.audio')
local events = require('haylen.events')
local jobs = require('haylen.jobs')
local system = require('haylen.system')

local Catalog = require('data.catalog')
local Random = require('random')
local Synth = require('synth')

local Player = {}

Player.queue = {}
Player.index = 0
Player.playing = false
Player.shuffle = false
Player.repeatMode = 'off'
Player.cacheSize = 6
Player.cache = {}
Player.cacheOrder = {}
Player.random = Random(5)
Player.sleepLeft = nil

function Player:start()
    events.on('mediaCommand', function(event)
        if event.command == 'play' or event.command == 'pause' or event.command == 'togglePlayPause' then
            Player.toggle()
        elseif event.command == 'next' then
            Player.next()
        elseif event.command == 'previous' then
            Player.previous()
        elseif event.command == 'seek' then
            Player.seek(event.position)
        end
    end, {owner = Player})
end

function Player.current()
    return Player.queue[Player.index]
end

function Player.changed()
    events.emit('playerChanged')
end

-- Plays a list of songs from one of them, which becomes the queue.
function Player.playList(tracks, first)
    Player.queue = {}
    for index, track in ipairs(tracks) do
        Player.queue[index] = track
    end
    Player.index = first or 1
    if Player.shuffle then
        Player.shuffleQueue()
    end
    Player.load()
end

-- Moves every song after the one that plays into a random order.
function Player.shuffleQueue()
    local queue = Player.queue
    for index = #queue, Player.index + 2, -1 do
        local other = Player.random:integer(Player.index + 1, index)
        queue[index], queue[other] = queue[other], queue[index]
    end
end

-- Makes the song that should play, or takes it from the cache, and starts it from `startAt` seconds.
function Player.load(startAt)
    local track = Player.current()
    if not track then
        return
    end
    -- The song that played stops as soon as another one is chosen, so its time never counts for the next one while that one is made.
    if Player.voice and Player.voiceTrack ~= track then
        audio.stop(Player.voice, 0.25)
        Player.voice = nil
    end
    Player.playing = true
    Player.position = startAt or 0
    Player.loading = Player.cache[track.id] == nil
    Player.changed()
    async.spawn(function()
        local sound = Player.sound(track):await()
        if Player.current() ~= track then
            return
        end
        Player.loading = false
        Player.voice = audio.playMusic(sound, {loop = Player.repeatMode == 'one', fade = 0.25, startAt = startAt or 0})
        Player.voiceTrack = track
        if not Player.playing then
            audio.pause(Player.voice)
        end
        Player.showNowPlaying()
        Player.changed()
        local upcoming = Player.queue[Player.index + 1]
        if upcoming then
            Player.sound(upcoming)
        end
    end)
end

-- Returns a promise of the sound of a song, which a job of the synthesizer makes the first time.
function Player.sound(track)
    local cached = Player.cache[track.id]
    if cached then
        return cached
    end
    local promise = jobs.spawn(function()
        local bytes = Synth.render(track.seed, Catalog.genre(track.genre).style)
        return audio.newSound(bytes, {format = 'int16', sampleRate = Synth.rate, channels = 1})
    end, {owner = Player})
    Player.cache[track.id] = promise
    Player.cacheOrder[#Player.cacheOrder + 1] = track.id
    if #Player.cacheOrder > Player.cacheSize then
        Player.cache[table.remove(Player.cacheOrder, 1)] = nil
    end
    return promise
end

function Player.toggle()
    if not Player.current() then
        return
    end
    Player.playing = not Player.playing
    if Player.voice then
        if Player.playing then
            audio.resume(Player.voice)
        else
            audio.pause(Player.voice)
        end
    end
    Player.showNowPlaying()
    Player.changed()
end

function Player.next()
    if Player.index < #Player.queue then
        Player.index = Player.index + 1
    elseif Player.repeatMode == 'all' and #Player.queue > 0 then
        Player.index = 1
    else
        Player.playing = false
        if Player.voice then
            audio.pause(Player.voice)
        end
        Player.changed()
        return
    end
    Player.load()
end

-- Starts the song again after its first seconds, and goes to the one before otherwise.
function Player.previous()
    if Player.elapsed() > 3 or Player.index <= 1 then
        Player.seek(0)
        return
    end
    Player.index = Player.index - 1
    Player.load()
end

function Player.seek(seconds)
    if Player.voice and audio.active(Player.voice) then
        audio.setCursor(Player.voice, seconds)
        Player.showNowPlaying()
        Player.changed()
    else
        Player.load(seconds)
    end
end

function Player.setShuffle(enabled)
    Player.shuffle = enabled
    if enabled then
        Player.shuffleQueue()
    end
    Player.changed()
end

-- Turns repeat from off to the whole queue to the one song, and back.
function Player.cycleRepeat()
    Player.repeatMode = ({off = 'all', all = 'one', one = 'off'})[Player.repeatMode]
    if Player.voice then
        Player.load(Player.elapsed())
    end
    Player.changed()
end

function Player.elapsed()
    if Player.voice and audio.active(Player.voice) then
        return audio.cursor(Player.voice)
    end
    return Player.position or 0
end

function Player.addToQueue(track, next)
    if #Player.queue == 0 then
        Player.playList({track}, 1)
        return
    end
    table.insert(Player.queue, next and Player.index + 1 or #Player.queue + 1, track)
    Player.changed()
end

function Player.removeFromQueue(index)
    if index == Player.index then
        return
    end
    table.remove(Player.queue, index)
    if index < Player.index then
        Player.index = Player.index - 1
    end
    Player.changed()
end

function Player.moveInQueue(from, to)
    local track = table.remove(Player.queue, from)
    table.insert(Player.queue, to, track)
    if from == Player.index then
        Player.index = to
    elseif from < Player.index and to >= Player.index then
        Player.index = Player.index - 1
    elseif from > Player.index and to <= Player.index then
        Player.index = Player.index + 1
    end
    Player.changed()
end

-- Stops the music after some minutes, or at the end of the song with 'track'.
function Player.setSleep(value)
    Player.sleepLeft = value
    Player.changed()
end

function Player.showNowPlaying()
    local track = Player.current()
    if not track then
        return
    end
    system.setNowPlaying({title = track.name, artist = track.artistName, album = track.albumName, duration = track.length, position = Player.elapsed(), playing = Player.playing, commands = {'play', 'pause', 'togglePlayPause', 'next', 'previous', 'seek'}})
end

function Player:update(dt)
    local track = Player.current()
    if not track or not Player.playing or Player.loading then
        return
    end
    if type(Player.sleepLeft) == 'number' then
        Player.sleepLeft = Player.sleepLeft - dt
        if Player.sleepLeft <= 0 then
            Player.sleepLeft = nil
            Player.toggle()
            events.emit('notice', 'The sleep timer paused the music.')
            return
        end
    end
    if Player.voice and Player.repeatMode ~= 'one' and (not audio.active(Player.voice) or audio.cursor(Player.voice) >= track.length - 0.05) then
        if Player.sleepLeft == 'track' then
            Player.sleepLeft = nil
            Player.playing = false
            Player.changed()
            return
        end
        Player.voice = nil
        Player.next()
    end
end

return Player
