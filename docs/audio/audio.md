# 🔊 Audio

This guide explains how a Haylen app plays sound. It covers sounds and how they load, voices, buses, music, the game pause, effects, loudness, positional audio, and how each platform shares the speaker with the system and with other apps. Read it before you add sound effects, music or a volume screen to an app. The [`haylen.audio` reference](../lua-api/audio.md) lists every function, option and error.

Haylen mixes sound with the [miniaudio](https://miniaud.io) library. Every audio call runs on the frame thread and returns at once. The mixing itself runs on the audio thread of the platform, which Lua code never touches. When the app has [no audio output](#without-an-audio-output), the mixing runs on the frame thread instead. The [architecture guide](../core/architecture.md) describes the threads.

## How the pieces fit

Six ideas make up the audio of an app.

- A **sound** is the audio data of a file, such as `jump.wav`. Load it once and play it many times.
- A **voice** is one playback of a sound. It has its own volume, pitch, pan, place and cursor, the point in the sound where it plays.
- A **bus** is a group of voices, such as all the music or all the interface sounds. A volume slider of a settings screen sets the volume of a bus.
- An **effect** changes the sound of a bus or of a voice, such as a filter, an echo or a room.
- The **listener** is the point in the world that hears. Positional voices fade and pan by their distance to it.
- The **output** is the audio device of the platform, which plays the mix.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

-- One sound, two voices on the bus "sfx", and a room effect on the whole bus.
local hit = assets.load('sfx/hit.wav')
audio.play(hit)
audio.play(hit, {volume = 0.5, pitch = 1.3})
audio.addBusEffect('sfx', audio.newEffect('reverb', {roomSize = 0.4, wet = 0.2}))
```

## Sounds

Sounds are assets of the type `sound` of [`haylen.assets`](../lua-api/assets.md). They load from WAV, Ogg Vorbis, MP3 and FLAC files. An app holds a sound in one of two ways.

| Way | How to load | Memory | Use it for |
| --- | --- | --- | --- |
| Decoded | `assets.load('audio/ui/click.ogg')` | The whole sound, as 32-bit float samples. | Short effects that play often. A new voice reads samples that are already in memory. |
| Streamed | `assets.load('audio/music/day.mp3', 'sound', {stream = true})` | A reader of the encoded file, and for each voice a window of encoded bytes and about one second of decoded sound. | Music and long ambience of any length, which would take a lot of memory decoded. |

A decoded sound decodes when its asset loads. It decodes on worker threads when it loads through `assets.loadAsync` or a preload group. A streamed sound reads only the header of its file when it loads. The entries of a preload group take the same options, so a group can preload music as a stream.

```json
{
    "groups": {
        "gameplay": [
            "audio/effects/",
            "audio/ambient/fire-loop.ogg",
            {"path": "audio/music/day.mp3", "options": {"stream": true}}
        ]
    }
}
```

A loaded sound is a [`Sound`](../lua-api/audio.md#sound) value, which tells its length, channels and sample rate. Two handles of the same loaded sound compare equal with `==`.

### Streamed sounds

A streamed sound never holds its whole file. It keeps a reader of the package, which reads the file in ranges. Every voice of the sound decodes the file on its own as it plays, so a track of any length plays in the same small memory.

1. Each voice reads the encoded file through a window of 64 KiB.
2. The I/O pool, a set of worker threads for slow input and output, decodes that window into a ring of about one second of sound ahead of the cursor.
3. Once per frame, the mixer asks the I/O pool to fill every ring that has room.
4. The audio thread only takes sound from the ring. It never reads the package, decrypts or waits.

When a ring runs dry, the voice plays silence until the sound arrives. That happens only when the frame thread stalls for longer than the ring lasts. A voice that seeks plays silence until the I/O pool decoded the sound at the new cursor. A looping voice goes from the end of the file to its start without a gap, because the ring already holds the start when the end plays.

Folders, zip archives and protected releases all stream. A protected release decrypts and decompresses only the chunks around the cursor of a voice, one at a time. So a streamed track never exists decrypted as a whole, in memory or on disk. A read that fails, such as a damaged chunk of a release, ends the voice and reports an engine error.

Every format streams, with two differences:

- MP3 encoders usually write a Xing or Info header with the length of the file. An MP3 file without one is read through once when the sound loads, since only its frames tell its length.
- Ogg Vorbis seeks by decoding forward, and a seek back decodes again from the start of the file. So a voice of a long Ogg Vorbis track that often seeks back reads more of the file than a voice of the other formats.

### Sounds made at run time

A sound can also come from bytes in memory, such as a recording that a plugin returns, or audio that the app downloaded or made itself. The function [`audio.newSound(bytes, options)`](../lua-api/audio.md#audionewsoundbytes-options) decodes the bytes of a WAV, Ogg Vorbis, MP3 or FLAC file, or streams them with `stream = true`. It also takes raw samples, as 32-bit floats or 16-bit integers, with their sample rate and channels.

```lua
local audio = require('haylen.audio')

-- Half a second of a falling tone, made in Lua as 32-bit float samples.
local rate, samples = 44100, {}
for index = 1, rate // 2 do
    local frequency = 880 - index / rate * 800
    samples[index] = string.pack('<f', math.sin(index * 2 * math.pi * frequency / rate) * 0.3)
end
audio.play(audio.newSound(table.concat(samples), {format = 'float32', sampleRate = rate, channels = 1}))
```

### Audio streams of plugins

The native part of a plugin, such as a microphone, a speech synthesizer or a radio of the network, feeds an audio stream that the app plays as a voice. The stream works like this:

- Native code pushes samples from any thread into a lock-free ring of the stream, in its own sample rate, channels and format.
- The voice reads the ring on the audio thread. It turns the samples into floats, resamples them to the rate of the mixer and spreads them over its channels.
- A voice that finds fewer samples than it needs plays silence for the rest and counts an underrun. It never ends by itself.
- The voice takes the options of `audio.play` that make sense for a live stream. So it plays through a bus, follows the pause through its process mode, takes effects and can be positional. A loop, a start time and a pitch variation mean nothing for it.
- The ring has one reader, so a new voice of a stream takes it over from the voice that played it before.
- The app can also read the newest samples into a float buffer, such as for a level meter.

The [`haylen.platform` reference](../lua-api/platform.md#audio-streams) describes the Lua side, and the [plugin guide](../platform/plugins.md#streams) shows how native code pushes samples.

## Voices

The function [`audio.play(sound, options)`](../lua-api/audio.md#audioplaysound-options) starts a voice and returns its id, an integer. Keep the id to change or stop the voice later. Options set the bus, volume, pitch, pan, loop, fade in, start time, place, process mode, effects and priority of the voice.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local crackle = audio.play(assets.load('audio/ambient/fire-loop.ogg'), {bus = 'ambience', loop = true, fadeIn = 1, volume = 0.7})
audio.setVolume(crackle, 0.4)
audio.stop(crackle, 0.5)
```

These rules make voices safe to use from any code:

- A voice id stays safe after the voice ends. A call with the id of a voice that ended does nothing, and `audio.active` returns `false`. So code never has to check a voice before it stops it.
- A paused voice keeps its cursor and still counts as active. A stopped voice stops counting as active at once, even while it fades out.
- Volumes, pitches, pans and places must be finite numbers, and pitches must be above `0`. So a stray `0 / 0` raises an error at once, instead of silencing a bus and its effects for good.
- The engine releases the voices that ended once per frame. Until then, `audio.voiceCount()` still counts them.
- Every stop, pause and steal fades over at least 5 milliseconds, the constant `Mixer::kDeclickSeconds`. Cutting a sound in the middle of a wave would click. A voice that resumes fades in over the same short ramp.

### Pitch variation

A sound that repeats, such as footsteps, hits and clicks, sounds mechanical when every play is the same. The option `pitchVariation` picks a new pitch for each play, at random around `pitch`. The function `audio.pitch(voice)` returns the pitch that was picked. The function `audio.seedVariation(seed)` makes the picks repeat for the same seed, which replays and tests need.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local step = assets.load('audio/effects/footstep-1.ogg')
audio.play(step, {volume = 0.35, pitchVariation = 0.08})
```

### Too many voices

At most 128 voices play at once. When every voice is busy, a new voice stops a voice that is not music. It picks the voice with the lowest `priority`, then the quietest of those, then the oldest. Give dialogue and interface sounds a high priority, so they outlast the sounds that matter less.

A sound that many objects trigger in the same frame, such as fifty coins at once, would stack into one loud blast. The function [`audio.setLimit(sound, limit)`](../lua-api/audio.md#audiosetlimitsound-limit) bounds one sound: how many of its voices play at once, the time between two of its plays, and which voice a new play replaces.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local coin = assets.load('sfx/coin.wav')
audio.setLimit(coin, {voices = 4, cooldown = 0.03, steal = 'quietest'})
local line = audio.play(assets.load('voice/line-01.ogg'), {priority = 220})
print(audio.active(line))
```

## Buses

Every voice plays through a bus, and buses form a tree. The root bus, `master`, feeds the output. The buses `music`, `sfx`, `ui` and `ambience` exist from the start as children of `master`. A voice plays on `sfx` and music plays on `music`, unless the options name another bus.

```text
master
├── ambience
├── music
├── sfx
└── ui
```

The volume of a voice is multiplied by the volume of its bus and of every bus above it. So one call changes a whole group of sounds, which is what a volume slider needs. A bus also has a mute that keeps its volume, a [process mode](#pause-and-process-modes) that its voices inherit, and a chain of [effects](#effects) that processes everything it mixes. Add buses of your own with `audio.createBus(name, parent)`, such as `footsteps` under `sfx` or `voices` under `master`. The [reference](../lua-api/audio.md#buses) lists every bus function.

```lua
local audio = require('haylen.audio')

audio.createBus('footsteps', 'sfx')
audio.setBusVolume('music', 0.5)
audio.setBusMuted('ui', true)

-- Fades the whole mix out over a second and a half.
audio.setBusVolume('master', 0, 1.5)
```

The module [`haylen.preferences`](../lua-api/preferences.md) saves the volume and the mute of every bus. The function `preferences.capture()` stores them in the groups `audio.volume` and `audio.muted`, under the name of each bus, such as `audio.volume.music`. A bus whose name holds a dot keeps its own entry in the group too. The function `preferences.apply()` restores them, and it skips the buses that do not exist yet. So create your own buses before you call `preferences.apply()`.

```lua
local audio = require('haylen.audio')
local preferences = require('haylen.preferences')

audio.createBus('voices')
preferences.apply()
```

## Music

Music plays one track at a time on a voice of its own. The function [`audio.playMusic(sound, options)`](../lua-api/audio.md#audioplaymusicsound-options) starts a track and crossfades from the track before over `fade` seconds, `1` by default: the old track fades out while the new one fades in. Music loops unless `loop = false`, and it plays on the `music` bus unless `bus` names another. Load long tracks as streams.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local calm = assets.load('music/calm.ogg', 'sound', {stream = true})
local battle = assets.load('music/battle.ogg', 'sound', {stream = true})

audio.playMusic(calm, {volume = 0.8})
local music = audio.playMusic(battle, {fade = 2})

-- Only the music pauses, while the effects and the interface sounds keep playing.
audio.pause(music)
audio.resume(music)
```

- The function `audio.playMusic` returns the voice id of the track, which every voice function takes. So `audio.pause` and `audio.resume` pause the music alone, and `audio.setVolume`, `audio.addEffect` and `audio.stop` reach the track.
- A call for the track that already plays keeps it going on the same voice, paused or not, and only changes its volume. So a scene can call `playMusic` in its `enter` hook, and the music never restarts when the player comes back to the scene.
- A paused track that the next one replaces stops without sounding again, and the next track plays.
- A track that cannot play raises an error and leaves the current track playing.
- The function `audio.stopMusic(fadeOut)` fades the track out over 1 second by default, and `audio.stopMusic(0)` stops it at once.
- The function `audio.music()` returns the sound that plays or is paused, or `nil`. A track stopped through its voice counts as over.
- The music voice is the last one that the voice limit stops.

### Intro loops

Many tracks start with an intro and then repeat a loop. Keep both in one file and give the loop points in seconds with `loopStart` and `loopEnd`. The voice plays from its start to `loopEnd` once, then repeats from `loopStart`. The jump is exact to the sample, without a gap, also for streamed tracks, which decode the start of the loop right after its end. Ogg Vorbis and WAV loop without a seam. MP3 encoders pad the start and the end of a file, and the decoder cannot always trim that padding, so use the other formats for loops that must be seamless.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local battle = assets.load('music/battle.ogg', 'sound', {stream = true})
audio.playMusic(battle, {loopStart = 8.0, loopEnd = 72.0})
```

## Pause and process modes

The game pause of `haylen.setPaused` reaches sounds through process modes, the same modes that scenes, timers and tweens use, as the [lifecycle guide](../core/lifecycle.md#pause-and-process-modes) explains. A voice plays only while its process mode runs in the current pause state. A voice that the pause stops keeps its cursor and still counts as active. It plays on from where it stopped when the pause ends.

Every bus has a process mode that its voices inherit. A bus in `'inherit'` takes the mode of its parent, and `master` counts `'inherit'` as `'pausable'`. The buses start with the modes most games want: effects and ambience stop with the game, while music and interface sounds keep playing. So a pause menu keeps its music and its clicks.

| Bus | Mode |
| --- | --- |
| `master` | `'inherit'`, which counts as `'pausable'` |
| `music` | `'always'` |
| `ui` | `'always'` |
| `sfx` | `'pausable'` |
| `ambience` | `'pausable'` |
| Buses of the app | `'inherit'`, the mode of their parent |

Change the mode of a bus with `audio.setBusProcessMode(bus, mode)`, or give one voice a mode of its own with the option `processMode` of `audio.play`. A countdown that must keep ticking through the pause plays as `'always'`. The music of a pause menu plays as `'whenPaused'`. A game whose music should stop with it moves the `music` bus to `'pausable'`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local haylen = require('haylen')

audio.setBusProcessMode('music', 'pausable')
audio.playMusic(assets.load('music/level.ogg', 'sound', {stream = true}))
audio.play(assets.load('sfx/fuse.wav'), {loop = true, processMode = 'always'})
local menuMusic = audio.play(assets.load('music/pause.ogg'), {bus = 'music', loop = true, processMode = 'whenPaused'})

-- The level music and the effects stop, the fuse keeps burning and the pause music starts.
haylen.setPaused(true)
print(audio.processMode(menuMusic), audio.busProcessMode('sfx'))
```

Voices also pause for reasons of their own. Each reason holds a voice apart from the others, so the end of one pause never resumes a voice that another reason still holds.

- The call `audio.pause(voice)` is the pause of one voice by the app, and `audio.paused(voice)` reads it.
- The function `audio.pauseAll()` pauses every voice that exists at that moment, such as while a cutscene or a dialog takes over. The function `audio.resumeAll()` resumes them. A voice that the app paused by itself stays paused, and voices that started in between play as usual.
- A system [interruption](#interruptions) pauses every voice for a reason of its own in the same way.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local radio = audio.play(assets.load('sfx/radio.ogg'), {bus = 'ambience', loop = true})
local rain = audio.play(assets.load('sfx/rain.ogg'), {bus = 'ambience', loop = true})
audio.pause(radio)

audio.pauseAll()
audio.play(assets.load('sfx/dialog-open.wav'), {bus = 'ui'})
audio.resumeAll()

-- The rain plays again, and the radio that the player turned off stays off.
print(audio.paused(radio), audio.paused(rain))
```

The function `audio.busStats()` tells, for every bus, how many voices play through it, how many play and how many are paused, and whether its mode runs in the current pause state. Use it in debug overlays and tests.

```lua
local audio = require('haylen.audio')

for _, bus in ipairs(audio.busStats()) do
    print(bus.name, bus.voices, bus.playing, bus.paused, bus.processing)
end
```

## Effects

Effects process sound inside the mixer. An effect on a bus processes everything the bus mixes. An effect on a voice processes that voice before its bus. The function [`audio.newEffect(kind, options)`](../lua-api/audio.md#audioneweffectkind-options) makes one, and the reference lists the parameters and defaults of each kind.

| Kind | What it does | Use it for |
| --- | --- | --- |
| `'lowpass'` | Removes the highs above the cutoff. | A sound behind a wall or underwater, or the game under a menu. |
| `'highpass'` | Removes the lows below the cutoff. | A radio, a phone or a thin intercom. |
| `'bandpass'` | Keeps a band around the cutoff, narrower as `q` grows. | A voice through a tin can. |
| `'notch'` | Removes a band around the cutoff. | A hum. |
| `'peak'` | Lifts or cuts a band around the cutoff by `gain` decibels. | One band of an equalizer. |
| `'lowShelf'`, `'highShelf'` | Lifts or cuts everything below or above the cutoff by `gain` decibels. | The bass and treble of an equalizer. |
| `'delay'` | Repeats the sound after `time` seconds, each repeat softer than the one before. | Echoes of a canyon, or a short slapback. |
| `'reverb'` | Places the sound in a space, from a small room to a large hall. | Caves, halls and corridors. |
| `'limiter'` | Keeps every sample under a level. | A bus that must never clip, as [loudness](#loudness-and-latency) explains. |

The filters are the standard second-order filters of the Audio EQ Cookbook. The parameter `cutoff` is in hertz. The default `q` of `0.7071` gives the flattest response without a bump. The delay keeps up to `maxTime` seconds of sound, at most 10. The reverb follows the Freeverb design: comb filters in parallel and allpass filters in series.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

-- A cave, where every sound effect rings.
audio.addBusEffect('sfx', audio.newEffect('reverb', {roomSize = 0.85, damping = 0.3, wet = 0.4}))

-- One shout with an echo of its own.
local echo = audio.newEffect('delay', {time = 0.35, feedback = 0.45, wet = 0.6})
audio.play(assets.load('sfx/shout.wav'), {effects = {echo}})
```

- Effects of one bus or voice run in the order they were added. The functions `audio.busEffects(bus)` and `audio.effects(voice)` list them in that order.
- An effect processes one bus or one voice at a time. Adding it to a second one raises an error until it leaves the first. Put an effect that many sounds share on a bus, and give each voice that needs its own effect an effect of its own.
- A voice lets its echo and its reverb ring out after it ends, until they fall silent. Then the effect is free for another voice.

### Changing effects while they play

The parameters of an effect are properties that change at any time, such as `muffle.cutoff = 400`. The audio thread uses a new value from the next block it mixes. Tweens animate these properties in C++, without running Lua on every frame.

```lua
local audio = require('haylen.audio')
local events = require('haylen.events')
local tween = require('haylen.tween')

-- The music keeps playing through the pause, and it grows muffled while the game is paused.
local muffle = audio.newEffect('lowpass', {cutoff = 20000})
audio.addBusEffect('music', muffle)

events.on('paused', function()
    tween.to(muffle, 0.3, {cutoff = 500}, {processMode = 'always'})
end)
events.on('unpaused', function()
    tween.to(muffle, 0.3, {cutoff = 20000}, {processMode = 'always'})
end)
```

Haylen writes its effects as nodes of its own in the node graph of miniaudio. The filter and delay nodes that miniaudio brings change their parameters by rewriting their numbers from the calling thread while the audio thread reads them, and their delay cannot change its time. The effects of Haylen keep their parameters in atomic values, which the audio thread picks up safely on its next block. Their delay glides to a new time, which bends the pitch of its repeats like a tape delay instead of clicking.

## Loudness and latency

Many loud sounds at once add up past full scale and crackle. So the engine puts a [limiter](../lua-api/audio.md#limiter) on the `master` bus of every app. It keeps every sample under `0.9` and recovers over `0.1` seconds after a peak. Set the key `audio.limiter` of `app.json` to `false` to leave it out. The function `audio.newEffect('limiter', options)` makes more limiters for any bus or voice.

Latency is the time from a tap to its sound. The key `audio.latency` of `app.json` trades it for power with `'low'`, `'balanced'` or `'powerSaving'`, and `audio.outputLatency()` tells the seconds that the device buffers.

```json
{
    "name": "Drum Pad",
    "identifier": "com.haylen.drums",
    "audio": {"latency": "low", "limiter": true}
}
```

Browsers keep sound locked until the first tap, click or key on the page. Until then `audio.outputState()` returns `'locked'`, and the event `audioUnlocked` tells when the sound starts. A web app can show a hint to turn the sound on meanwhile.

```lua
local audio = require('haylen.audio')
local events = require('haylen.events')

local hint = {visible = audio.outputState() == 'locked'}
events.on('audioUnlocked', function()
    hint.visible = false
end, {owner = hint})
```

## Positional audio

A voice becomes positional when `audio.play` receives `x` or `y`, or when `audio.setPosition` moves it. The listener is a point in the world. It stays at `0, 0` until `audio.setListener(x, y)` moves it or `audio.followCamera(camera)` ties it to a camera. The engine updates every positional voice once per frame, after the scenes update. So it is enough to move the listener and the voices in `update`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local chop = assets.load('audio/effects/chop.ogg')
audio.setSpatialization({maxDistance = 1200, panDistance = 800, doppler = 1})

scene.push({
    enter = function(self)
        self.player = {x = 0, y = 0}
        self.camera = graphics2d.newCamera()
        audio.followCamera(self.camera)
    end,
    update = function(self, dt)
        self.camera:follow(self.player.x, self.player.y, dt)
    end,
    chop = function(self, tree)
        audio.play(chop, {x = tree.x, y = tree.y, pitchVariation = 0.08})
    end,
})
```

A followed camera moves the listener to its position plus its offset, without its shake. The engine keeps the camera alive while the listener follows it. The call `audio.followCamera(nil)` stops following and leaves the listener where it was.

Only the volume, the pan and the pitch change with the place. So a positional sound stays a plain stereo sound. When a game wants the muffling of distance or of walls, it adds a lowpass effect to the voice.

### How the volume falls

The function [`audio.setSpatialization(settings)`](../lua-api/audio.md#audiosetspatializationsettings) picks the fade model and its distances. Every model measures the distance `d` clamped between `minDistance` and `maxDistance`. A voice within `minDistance` plays at full volume.

| Model | Volume at distance `d` |
| --- | --- |
| `'linear'` | `1 - rolloff * (d - minDistance) / (maxDistance - minDistance)`. With a rolloff of `1`, it reaches silence at `maxDistance`. |
| `'inverse'` | `minDistance / (minDistance + rolloff * (d - minDistance))`. This is the natural fade of sound, which never reaches silence. |
| `'exponential'` | `(d / minDistance) ^ -rolloff`. |

The models `'inverse'` and `'exponential'` need a `minDistance` above `0`. They keep the volume they reach at `maxDistance` for every voice farther away. The `'linear'` model, the default, suits most 2D games, because a sound off the screen should fall silent.

### Pan and the Doppler effect

A voice pans by its horizontal offset from the listener, measured against `panDistance`, and adds its own `pan`. So a voice to the right sounds on the right, and a voice straight above or below the listener stays in the center.

The Doppler effect is the rise in pitch of a sound that comes closer and the fall of one that moves away. It is on when `doppler` is above `0`. The engine measures how fast the listener and each voice move toward each other along the line between them, from their movement between frames, so an app only moves them. The new pitch is `(speedOfSound + doppler * listenerSpeed) / (speedOfSound - doppler * voiceSpeed)`, where each speed counts toward the other side. It stays within two octaves up or down. The function `audio.pitch(voice)` keeps returning the pitch the app set, without the shift.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local scene = require('haylen.scene')

-- A car drives past the listener from left to right and its pitch drops as it passes.
audio.setSpatialization({doppler = 1, speedOfSound = 3430})
audio.setListener(0, 0)
local car = audio.play(assets.load('sfx/car.ogg'), {loop = true, x = -2000, y = 100})

scene.push({
    x = -2000,
    update = function(self, dt)
        self.x = self.x + 600 * dt
        audio.setPosition(car, self.x, 100)
    end,
})
```

## Sessions and interruptions

The output runs at 48000 Hz in stereo by default, as `audio.sampleRate()` and `audio.channels()` tell. The engine owns the audio context and the playback device. So it decides how the app shares the speaker with the system and with other apps.

### iOS and tvOS sessions

On iOS and tvOS, the system groups the sound of an app in an audio session. The key `audio.iosSession` of `app.json` picks its category, which decides what the silent switch and other apps do.

| `iosSession` | Silent switch | Other apps | Use it for |
| --- | --- | --- | --- |
| `'ambient'` | Silences the app. | Keep playing, mixed with the app. | Games, so a player can listen to their own music. This is the default. |
| `'soloAmbient'` | Silences the app. | Stop. | Games whose sound is part of the play. |
| `'playback'` | Keeps playing. | Stop, unless `mixWithOthers` is `true`. | Music players, radios, meditation and other apps that play audio as their purpose. |

```json
{
    "name": "Night Radio",
    "identifier": "com.haylen.nightradio",
    "audio": {"iosSession": "playback", "mixWithOthers": false}
}
```

The key `mixWithOthers` is `false` by default. Only the `'playback'` category takes `true`, because `'ambient'` always mixes and `'soloAmbient'` never does. An unknown category raises `The "audio.iosSession" value "<value>" in "app.json" is unknown. It is "ambient", "soloAmbient" or "playback".`, and `mixWithOthers` with another category raises `The "audio.mixWithOthers" option in "app.json" needs the "iosSession" value "playback".`. Closing the output leaves the iOS audio session active, so an app that restarts finds the session as it left it.

### Android

On Android the device plays through AAudio with the usage `game` and the content type `sonification`. That is how the system routes, ducks and mixes a game. The activity asks for the audio focus with the same attributes. Android has no categories like those of iOS, so `audio.iosSession` changes nothing there. An Android app takes the audio focus whatever its category, which pauses the music of another app while it plays in the foreground.

### Interruptions

The system can take the audio away, for a phone call, an alarm, Siri or another app. An interruption goes through these steps:

1. The engine stops the output and pauses every voice, for a reason of its own.
2. It publishes the event `audioInterrupted`, and `audio.interrupted()` returns `true` until the interruption ends.
3. When the interruption ends, the engine opens the audio device again. On iOS this makes the audio session active again with its category.
4. The engine starts the output again unless the app is in the background, resumes the voices and publishes `audioResumed`.

An interruption that ends while the app is not active waits until the app is active again, because iOS gives the audio back only to an active app. When the system still refuses the audio at that moment, the interruption ends all the same. The app then goes on [without an audio output](#without-an-audio-output) until the system gives the audio back.

When the output moves to another device, such as headphones that are unplugged, the engine publishes `audioRouteChanged`. A music player pauses its track there. When iOS, tvOS or Mac Catalyst reset their media services, every audio output of the app falls silent. The runtime reports this as the platform event `audioServicesReset`, and the engine opens the audio device again, as `audio.restartOutput()` does.

```lua
local audio = require('haylen.audio')
local events = require('haylen.events')
local haylen = require('haylen')

events.on('audioInterrupted', function()
    -- A call came in, so the run pauses too and the player comes back to the pause menu.
    haylen.setPaused(true)
end)

events.on('audioResumed', function()
    print('The audio is back.', audio.interrupted())
end)

events.on('audioRouteChanged', function()
    print('The headphones changed.')
end)
```

Interruptions reach the engine in two ways, and both take the same path:

- The miniaudio device reports the interruptions and route changes of the iOS audio session, and the route changes of AAudio.
- The platform reports what the device cannot see as the platform events `interruptionBegan` and `interruptionEnded`. These also make the app inactive while they last.

On Android the activity turns the audio focus into those platform events. A loss of focus, lasting or short, begins an interruption. A gain of focus ends it, including the focus that the activity asks for again when it comes back to the foreground. A short loss that lets the app duck does not interrupt the audio, since the system lowers its volume instead. When the device and the platform report the same interruption, the engine pauses and resumes once and publishes each event once. C++ hosts report device events of their own with `Mixer::reportDeviceEvent`, from any thread.

### The background

When the app goes to the background, the engine stops the audio output. It starts the output again when the app comes back, unless an interruption still holds it. On the web, a page that pauses the app with `Module.haylen.pause()` suspends the output the same way. The app stays suspended until the page resumes it and the tab shows.

### The browser

On the web, the engine plays through an audio output of its own, which is the only output that miniaudio has there. It works like this:

- The mixer runs on the page thread like the rest of the app and mixes blocks of 256 frames.
- The page posts the blocks over the message port of an `AudioWorkletNode`.
- Its processor, the file `haylen-audio-worklet.js` next to the runtime script, keeps them in a small ring and plays them on the audio thread of the browser.
- The processor answers with the frames it played, so the page mixes new blocks until 50 ms of sound wait ahead of the output again.

A frame of the app that takes much longer than usual therefore does not starve the output. The output needs no `SharedArrayBuffer`, no COOP or COEP headers and no WebAssembly threads. Browsers offer `AudioWorklet` only to pages served over `https` or from `localhost`. On any other page, such as one served over plain `http` from the network address of a computer, the app runs [without an audio output](#without-an-audio-output), and the console says why. The `audio` entry of [`onStats`](../tools/build.md#runtime-api) reports whether the output is available, its state, the sound waiting in it and the render quanta it played without samples.

Browsers keep audio silent until the player interacts with the page. The runtime resumes the output at every tap, click or key while the app plays in the foreground. This also brings the sound back after Safari on iOS suspends it for an interruption. Until the unlock, the output fills its 50 ms and then asks for no more blocks. So voices that started earlier, such as the menu music, begin when the player first clicks or taps. The engine adds no unlock screen of its own. A web app whose first screen matters for sound should wait for a click, as a title screen with a start button does.

## Without an audio output

An app never stops because it has no audio output. These cases leave the mixer without one:

- A computer without an audio device.
- An iOS audio session that another app holds.
- A web page served over plain `http`, where the browser offers no `AudioWorklet`.

The app runs as usual. The log writes one warning with the reason, and `audio.outputAvailable()` returns `false`, so an app can tell the player that its sound is off.

Without an output the mixer keeps time by itself. Every frame it mixes the time that passed and throws the samples away. So voices play to their end, loops repeat, fades and crossfades finish, effect tails ring out, the voice limit stops the voice with the lowest priority, and positional voices follow the listener, exactly as they would out loud. Time stands still for the audio while the app is in the background or an interruption holds it, as it does with an output.

An interruption that ends while the system still refuses the audio ends all the same. The voices resume, the engine publishes `audioResumed`, the log warns once and the app goes on without an output. Whenever the app becomes active and whenever an interruption ends, the engine tries again to open the output, without a new warning. So the sound comes back as soon as the system offers it.

The system may also take long to open the device, or never answer. For example, opening the device on the iOS simulator blocks inside AudioToolbox while the audio service of the Mac hangs, and the system ends an app that waits for it on its main thread. So the engine opens the device on a thread of its own, when the app starts and whenever it opens the output again. Meanwhile the frames go on without sound, as `audio.outputOpening()` tells, and the app plays sound from the first frame after the device opened. A device that does not answer within 3 seconds of frames makes the log write one warning, and the log tells when the device answers at last. Browsers have no threads, and the output of a page opens at once. Tests simulate a device that refuses to open or never answers with the output that `HeadlessHost::setAudioOutput` gives the headless host, as the [testing guide](../tools/testing.md) describes.

```lua
local audio = require('haylen.audio')
local events = require('haylen.events')

local function showSoundHint()
    if audio.hasDevice() and not audio.outputAvailable() then
        print('This device plays no sound right now.')
    end
end

showSoundHint()
events.on('appActive', showSoundHint)
```

## The Tiny Island sound module

The file `samples/games/tiny-island/source/systems/sound.lua` wraps `haylen.audio` in four functions that the rest of the game calls by name. It is a small model for the sound module of an app. Here it is with a few of its sounds and tracks.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local sound = {}

-- Every effect by name, with its bus, its recordings and an optional volume.
local effects = {
    click = {bus = 'ui', files = {'ui/click.ogg'}},
    chop = {bus = 'sfx', files = {'effects/chop.ogg', 'effects/wood-hit-1.ogg', 'effects/wood-hit-2.ogg', 'effects/wood-hit-3.ogg'}},
    footstep = {bus = 'sfx', files = {'effects/footstep-1.ogg', 'effects/footstep-2.ogg', 'effects/footstep-3.ogg', 'effects/footstep-4.ogg'}, volume = 0.35},
    dawn = {bus = 'ui', files = {'jingles/dawn.ogg'}},
}

local music = {menu = 'music/menu.mp3', day = 'music/day.mp3', gameOver = 'music/game-over.mp3', night = 'ambient/night.ogg'}

local function load(file)
    return assets.load('audio/' .. file)
end

-- Picks one recording, varies its pitch and places it in the world when the caller gives a position.
function sound.play(name, x, y)
    local effect = effects[name]
    local file = effect.files[math.random(#effect.files)]
    local options = {bus = effect.bus, volume = effect.volume or 1, pitchVariation = 0.08}
    if x then
        options.x = x
        options.y = y
    end
    return audio.play(load(file), options)
end

function sound.loop(file, volume)
    return audio.play(load(file), {bus = 'ambience', volume = volume, loop = true, fadeIn = 1})
end

function sound.music(name, fade)
    local track = assets.load('audio/' .. music[name], 'sound', {stream = true})
    audio.playMusic(track, {fade = fade or 1.5, loop = name ~= 'gameOver'})
end

function sound.stop(voice)
    audio.stop(voice, 0.5)
end

return sound
```

- Effects with several recordings, such as `chop` or `footstep`, pick one at random on each play.
- Gameplay sounds such as chopping, hits and footsteps pass their position, so they pan and fade with their distance to the player. Interface sounds pass none.
- The campfire starts its crackle with `sound.loop` on the `ambience` bus, with a fade in of one second, and `sound.stop` fades it out over half a second when the fire is destroyed.
- The menu plays the track `menu`, and a run starts with `day`. Dusk crossfades to the `night` ambience over 3 seconds, and dawn crossfades back. The game over track plays once without looping.

The rest of the game completes the picture:

- The file `systems/game.lua` moves the listener to the player every frame with `audio.setListener`, and keeps the default distances of 100 and 1500 world units.
- The file `systems/preferences.lua` turns the two volume sliders of the settings screen into bus volumes. The music slider sets the `music` bus, and the effects slider sets `sfx`, `ui` and `ambience`. The sliders call it on every frame they move, and the values reach storage when the settings screen closes.
- The file `content/preload.json` preloads the interface sounds in the `boot` group and the menu music as a stream in the `menu` group. It preloads the effects, the jingles, the ambience and the other tracks in the `gameplay` group, so no sound loads in the middle of a run.

## From C++

C++ code reaches the same audio through these classes:

- The class `haylen::audio::Mixer` of `haylen/audio/Mixer.hpp`, which `engine.getAudio()` returns, offers voices, buses, music, process modes, effects and positional audio. Its options are the structures `Mixer::PlayOptions`, `Mixer::MusicOptions` and `Mixer::Spatialization`. The method `Mixer::play` also takes a `std::shared_ptr<platform::AudioStream>` to play an audio stream.
- The class `haylen::audio::Sound` of `haylen/audio/Sound.hpp` holds decoded or streamed sound data. The method `Sound::decode` makes it from encoded bytes, `Sound::stream` from a `std::shared_ptr<io::PackageReader>` of an encoded file, such as the one `Package::openAssetReader` returns, and `Sound::fromSamples` from spans of float or 16-bit samples.
- The effects are `audio::Filter`, `audio::Delay`, `audio::Reverb` and `audio::Limiter`, made with `std::make_shared` and their settings structures.
- The class `audio::Session` is the audio session of `app.json`.

The engine applies its pause through `Mixer::setProcessPaused`. The method `Mixer::deviceEventReceived` delivers device events on the frame thread, and the audio plugin turns them into the interruptions and events above. The method `Mixer::isOutputAvailable` tells whether the mix reaches an audio device. A mixer made without a device mixes on demand through `render`, which tests and offline tools use. Every mixer takes the `core::JobSystem` whose I/O pool decodes its streamed voices.

```cpp
#include "haylen/audio/Filter.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Engine.hpp"

void muffleMusic(haylen::core::Engine& engine) {
    auto muffle = std::make_shared<haylen::audio::Filter>(haylen::audio::Filter::Kind::Lowpass, haylen::audio::Filter::Settings{.cutoff = 600.0F});
    engine.getAudio().addBusEffect("music", muffle);
    engine.getAudio().setSpatialization({.model = haylen::audio::Mixer::Spatialization::Model::Inverse, .minDistance = 50.0F});
}
```
