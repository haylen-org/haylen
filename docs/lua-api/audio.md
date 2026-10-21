# 📖 `haylen.audio`

The module `haylen.audio` plays sounds and music. Use it to play voices and music, group sounds into buses with effects, place sounds in the world and read the state of the audio output. The [audio guide](../audio/audio.md) explains voices, buses, process modes, positional audio and the sessions of each platform.

```lua
local audio = require('haylen.audio')
```

## Sounds and voices

A sound is the audio data of a file. Load it with [`haylen.assets`](assets.md) from a WAV, Ogg Vorbis, MP3 or FLAC file, or make it from bytes with [`audio.newSound`](#audionewsoundbytes-options). A sound loads decoded into memory by default, which suits short effects. The asset option `stream = true` keeps the file encoded and decodes it while it plays, which suits music of any length, as the [audio guide](../audio/audio.md#streamed-sounds) explains.

A voice is one playback of a sound. Every call to `audio.play` starts a new voice and returns its id, an integer. The same sound may play on many voices at once.

- A call with the id of a voice that already ended does nothing. So an app never has to check a voice before it stops it.
- At most 128 voices play at once. A new voice beyond that stops another voice that is not music: the one with the lowest `priority`, then the quietest of those, then the oldest.
- Every stop, pause and steal fades over at least 5 milliseconds, so a sound cut in the middle never clicks.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local jump = assets.load('sfx/jump.wav')
local theme = assets.load('music/theme.ogg', 'sound', {stream = true})

local voice = audio.play(jump)
audio.playMusic(theme)
print(audio.active(voice))
```

### audio.newSound(bytes, options)

Makes a [`Sound`](#sound) from bytes in memory, such as a recording that a plugin returns or audio the app downloaded or made itself. The string `bytes` holds either a whole WAV, Ogg Vorbis, MP3 or FLAC file, or raw samples when `options.format` is given. The table `options` is optional.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `stream` | boolean | `false` | Keeps a file encoded and decodes it while it plays, a small window at a time, like the `stream` option of assets. The string itself stays in memory. |
| `format` | string | none | Marks the bytes as raw samples: `'float32'` for 32-bit floats from -1 to 1, or `'int16'` for 16-bit integers, which become floats. |
| `sampleRate` | integer | none | The sample rate of raw samples in hertz. Raw samples need it. |
| `channels` | integer | none | The channels of raw samples. Raw samples need it. |

Raw samples are interleaved by channel, one sample for every channel in each frame. They use the byte order of the device, which is little-endian on every platform the engine runs on.

These errors can happen:

- Bytes that are not a sound file raise `Audio data is not a supported WAV, FLAC, MP3 or Ogg Vorbis file.`.
- A file or raw samples without a single sample raise `Audio data contains no samples.`.
- A `format` other than the two names raises `The format of raw samples is "float32" or "int16", not "<format>".`.
- Raw samples without `sampleRate` or `channels` raise `Raw audio needs a sample rate and channels.`.
- Bytes that do not fill whole samples raise `Samples of this format take <n> bytes each, which <m> bytes do not fill.`.
- Samples that do not fill whole frames raise `Raw audio samples come in whole frames, one sample for every channel.`.
- A `sampleRate` or `channels` given for a file raises `Only raw samples take a sample rate and channels, together with their format.`.
- Raw samples with `stream = true` raise `Raw samples are decoded already, so they cannot stream.`.

```lua
local async = require('async')
local audio = require('haylen.audio')
local platform = require('haylen.platform')

-- A quarter second of a 440 Hz tone, made in Lua as 16-bit samples at 22050 Hz.
local samples = {}
for index = 1, 5512 do
    samples[index] = string.pack('<h', math.floor(math.sin(index * 2 * math.pi * 440 / 22050) * 8000))
end
local beep = audio.newSound(table.concat(samples), {format = 'int16', sampleRate = 22050, channels = 1})
audio.play(beep)

async.spawn(function()
    -- The native part of a plugin answers with a recording as the bytes of a WAV file.
    local recording = platform.call('recorder.lastRecording'):await()
    if recording then
        audio.play(audio.newSound(recording.wav))
    end
end)
```

### audio.play(sound, options)

Starts a voice for `sound` and returns its id. The table `options` is optional.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `bus` | string | `'sfx'` | The [bus](#buses) the voice plays through. |
| `volume` | number | `1` | The volume of the voice, where `1` is the level of the file. |
| `pitch` | number | `1` | The speed and pitch, above `0`. The value `2` plays one octave higher and twice as fast. |
| `pitchVariation` | number | `0` | Picks the pitch of this play at random between `pitch - pitchVariation` and `pitch + pitchVariation`, so a sound that repeats never sounds exactly the same. It is at least `0` and smaller than `pitch`. |
| `pan` | number | `0` | The stereo position, from `-1` on the left to `1` on the right. |
| `loop` | boolean | `false` | Repeats the sound until the voice stops. |
| `fadeIn` | number | `0` | Seconds to fade in from silence. |
| `startAt` | number | `0` | Seconds into the sound where the voice starts. |
| `loopStart`, `loopEnd` | number | `0` and the length | The part of a looping voice that repeats, in seconds. The voice plays from its start to `loopEnd` once, then repeats from `loopStart` without a gap, also for streamed sounds. |
| `x`, `y` | number | none | A position in the world, which makes the voice [positional](#positional-audio). When only one is given, the other is `0`. |
| `processMode` | string | `'inherit'` | When the voice plays during the game pause: `'inherit'`, `'pausable'`, `'whenPaused'`, `'always'` or `'disabled'`. The mode `'inherit'` takes the mode of the bus. |
| `effects` | list | none | [Effects](#effects) from `audio.newEffect` that process the voice in order, before its bus. |
| `priority` | integer | `128` | From `0` to `255`. When every voice is busy, a new voice stops the voice with the lowest priority first, so dialogue and interface sounds outlast sounds that matter less. |

A call that raises an error stops no voice, even when every voice is busy. These errors can happen:

- An unknown key raises `Unknown option "<key>".`.
- An unknown bus raises `The audio bus "<name>" does not exist.`.
- A `volume`, `pan`, `startAt`, `x` or `y` that is not a finite number, such as `0 / 0` or `math.huge`, raises `Audio needs a finite volume.`, `Audio needs a finite pan.`, `Audio needs a finite start time.` or `Audio needs a finite position.`.
- A `pitch` that is not a finite number above `0` raises `Audio needs a finite pitch above 0.`.
- A bad `pitchVariation` raises `A pitch variation must be at least 0 and smaller than the pitch.`.
- A `fadeIn` below `0`, longer than a day or not a number raises `Audio needs a fade from 0 to 86400 seconds.`.
- Loop points outside the sound or in the wrong order raise `A loop needs 0 <= loopStart < loopEnd <= the length of the sound.`.
- Loop points closer than one frame of the sound raise `A loop needs to span at least one frame of the sound.`.
- A `priority` outside `0` to `255` raises `The priority of a voice runs from 0 to 255.`.
- An `effects` value that is not a list of effects raises `The option "effects" must be a list of audio effects.`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local step = assets.load('sfx/step.wav')
local fire = assets.load('sfx/fire.ogg')
local click = assets.load('ui/click.wav')

audio.play(step, {volume = 0.6, pitchVariation = 0.1})
audio.play(fire, {bus = 'ambience', loop = true, fadeIn = 2, x = 400, y = 120})
audio.play(click, {bus = 'ui', pan = -0.3, startAt = 0.02, priority = 200})
audio.play(click, {bus = 'ui', processMode = 'always', effects = {audio.newEffect('highpass', {cutoff = 400})}})
```

### audio.stop(voice, fadeOut)

Stops a voice. It fades the voice out over `fadeOut` seconds, `0` by default. A fade of `0` still ramps the voice down over 5 milliseconds, so it never clicks, and the effects of the voice stay with it until the ramp ends. A fade below `0`, longer than a day or not a number raises `Audio needs a fade from 0 to 86400 seconds.`, as every fade of this module does.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local motor = audio.play(assets.load('sfx/engine.wav'), {loop = true})
audio.stop(motor, 0.5)
```

### audio.stopAll(fadeOut)

Stops every voice, music included. It fades them out over `fadeOut` seconds, `0` by default.

```lua
local audio = require('haylen.audio')

audio.stopAll(0.3)
```

### audio.pause(voice)

Pauses a voice for the app. A paused voice keeps its cursor, the place it plays, and still counts as active.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local rain = audio.play(assets.load('sfx/rain.ogg'), {bus = 'ambience', loop = true})
audio.pause(rain)
```

### audio.resume(voice)

Ends the pause of `audio.pause`. The voice plays on from where it stopped, unless another reason still holds it, such as the game pause.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local rain = audio.play(assets.load('sfx/rain.ogg'), {bus = 'ambience', loop = true})
audio.pause(rain)
audio.resume(rain)
```

### audio.paused(voice)

Returns `true` while the app holds the voice with `audio.pause`. The game pause, `audio.pauseAll` and interruptions do not count, because they hold the voice for reasons of their own. A voice that ended returns `false`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local radio = audio.play(assets.load('sfx/radio.ogg'), {loop = true})
audio.pause(radio)
audio.pauseAll()
audio.resumeAll()
print(audio.paused(radio)) -- true
```

### audio.pauseAll()

Pauses every voice that exists now, music included, such as while a cutscene or a dialog takes over. Voices that start afterwards play as usual.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

audio.play(assets.load('sfx/rain.ogg'), {bus = 'ambience', loop = true})
audio.pauseAll()
audio.play(assets.load('ui/open.wav'), {bus = 'ui'})
```

### audio.resumeAll()

Resumes the voices that `audio.pauseAll` paused. A voice stays paused when the app paused it with `audio.pause`, or when the game pause or an interruption holds it.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local radio = audio.play(assets.load('sfx/radio.ogg'), {loop = true})
local rain = audio.play(assets.load('sfx/rain.ogg'), {bus = 'ambience', loop = true})
audio.pause(radio)
audio.pauseAll()
audio.resumeAll()
print(audio.paused(radio), audio.paused(rain)) -- true false
```

### audio.active(voice)

Returns `true` while the voice plays or is paused. It returns `false` once the voice ended or was stopped, even while `audio.stop` still fades it out.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local scene = require('haylen.scene')

local intro = audio.play(assets.load('sfx/intro.wav'))

scene.push({
    update = function(self, dt)
        if not self.done and not audio.active(intro) then
            self.done = true
            print('The intro finished.')
        end
    end,
})
```

### audio.voiceCount()

Returns how many voices exist. The engine releases the voices that ended once per frame, so a voice that just ended counts until the end of the frame.

```lua
local audio = require('haylen.audio')

print(audio.voiceCount())
```

### audio.processMode(voice)

Returns the process mode that the voice was played with. It is `'inherit'` unless `audio.play` named another. A voice that ended returns `'inherit'`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local tick = audio.play(assets.load('sfx/tick.wav'), {loop = true, processMode = 'always'})
print(audio.processMode(tick)) -- always
```

### audio.setVolume(voice, volume)

Changes the volume of a voice. A volume that is not finite raises `Audio needs a finite volume.`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local wind = audio.play(assets.load('sfx/wind.ogg'), {bus = 'ambience', loop = true})
audio.setVolume(wind, 0.25)
```

### audio.setPitch(voice, pitch)

Changes the pitch and speed of a voice. A pitch that is not a finite number above `0` raises `Audio needs a finite pitch above 0.`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local scene = require('haylen.scene')

local motor = audio.play(assets.load('sfx/motor.wav'), {loop = true})
local speed = 0

scene.push({
    update = function(self, dt)
        speed = math.min(1, speed + dt * 0.2)
        audio.setPitch(motor, 0.8 + speed)
    end,
})
```

### audio.pitch(voice)

Returns the pitch of a voice, with the random part that `pitchVariation` picked. It leaves out the Doppler shift of [positional audio](#positional-audio). A voice that ended returns `0`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local voice = audio.play(assets.load('sfx/coin.wav'), {pitch = 1.2, pitchVariation = 0.1})
print(audio.pitch(voice))
```

### audio.seedVariation(seed)

Seeds the random generator behind `pitchVariation` with an integer from `0`. The same seed picks the same pitches again, which replays and tests need. A negative seed raises a bad argument error with `expected a non-negative integer`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local step = assets.load('sfx/step.wav')
audio.seedVariation(42)
for _ = 1, 3 do
    print(audio.pitch(audio.play(step, {pitchVariation = 0.1})))
end
```

### audio.setPan(voice, pan)

Changes the stereo position of a voice, from `-1` on the left to `1` on the right. A positional voice adds the pan of its place to this value. A pan that is not finite raises `Audio needs a finite pan.`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local voice = audio.play(assets.load('sfx/bird.wav'))
audio.setPan(voice, 0.8)
```

### audio.setPosition(voice, x, y)

Moves a voice to a place in the world and makes it positional. The engine updates the volume, pan and pitch of positional voices every frame. A coordinate that is not finite raises `Audio needs a finite position.`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local scene = require('haylen.scene')

local buzz = audio.play(assets.load('sfx/bee.wav'), {loop = true, x = 0, y = 0})
local time = 0

scene.push({
    update = function(self, dt)
        time = time + dt
        audio.setPosition(buzz, math.cos(time) * 300, math.sin(time) * 150)
    end,
})
```

### audio.cursor(voice)

Returns the place where a voice plays, in seconds from the start of the sound. A voice that ended returns `0`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local speech = audio.play(assets.load('voice/line-01.ogg'))
print(audio.cursor(speech))
```

### audio.setCursor(voice, seconds)

Moves a voice to a time in seconds, such as when a player drags the progress bar of a song. The time stays between `0` and the length of the sound. A voice that does not loop ends once its cursor reaches the end. A paused voice keeps the new cursor and plays from it when it resumes. The id of a voice that ended does nothing.

A time that is not finite raises `Audio needs a finite cursor.`. The voice of a live audio stream plays its samples as they arrive, so it raises `The voice of a live audio stream has no cursor to move.`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local track = audio.playMusic(assets.load('music/album-03.ogg', 'sound', {stream = true}), {loop = false})
audio.setCursor(track, 95)
```

### audio.setLimit(sound, limit)

Bounds how one sound plays, so a sound that many objects trigger in one frame never stacks into a blast. The table `limit` holds these keys.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `voices` | integer | `0` | How many voices of the sound play at once. The value `0` sets no bound. |
| `cooldown` | number | `0` | The seconds of app time between two plays of the sound. |
| `steal` | string | `'oldest'` | What a play beyond `voices` does: `'oldest'` stops the oldest voice of the sound, `'quietest'` stops the quietest one, and `'none'` starts nothing. |

A play that the limit refuses returns the id of a voice that already ended, so the calls that follow it do nothing. A limit without `voices` and without `cooldown` removes the limit. A limit lasts as long as the sound exists.

An unknown key raises `Unknown option "<key>".`. A bad `steal` raises `The "steal" of a sound limit must be "oldest", "quietest" or "none".`. A negative or infinite cooldown raises `The cooldown of a sound must be a finite number of seconds from 0.`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

-- Fifty coins collected in one frame play as four voices at most.
local coin = assets.load('sfx/coin.wav')
audio.setLimit(coin, {voices = 4, cooldown = 0.03, steal = 'oldest'})
for _ = 1, 50 do
    audio.play(coin, {pitchVariation = 0.05})
end
```

### audio.limit(sound)

Returns the limit of a sound as a table `{voices, cooldown, steal}`, or `nil` when it has none.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local coin = assets.load('sfx/coin.wav')
audio.setLimit(coin, {voices = 4})
print(audio.limit(coin).voices, audio.limit(coin).steal) -- 4 oldest
```

## Music

Music plays one track at a time, on a voice of its own. When every voice is busy, the other voices stop before the music voice. The [audio guide](../audio/audio.md#music) explains crossfades and intro loops.

### audio.playMusic(sound, options)

Starts a music track, crossfades from the track before and returns the voice id of the new track. That id works with every voice function: `audio.pause` and `audio.resume` pause the music alone, `audio.setCursor` moves through the track, and `audio.setVolume`, `audio.addEffect` and `audio.stop` reach the track. The table `options` is optional.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `bus` | string | `'music'` | The bus the track plays through. |
| `volume` | number | `1` | The volume of the track. |
| `fade` | number | `1` | The seconds of the crossfade. The old track fades out while the new one fades in. |
| `loop` | boolean | `true` | Repeats the track. |
| `startAt` | number | `0` | Seconds into the track where it starts, such as where the player left a song. |
| `loopStart`, `loopEnd` | number | `0` and the length | The part of a looping track that repeats, so one file plays its intro once and repeats the rest without a gap. |

- When the track already plays or is paused, it keeps its voice, which the call returns. Only its volume changes.
- A paused track that the next track replaces stops without sounding again, and the next track plays.
- A track that cannot play, such as one for an unknown bus, raises the errors of [`audio.play`](#audioplaysound-options) and leaves the current track playing.
- An unknown key raises `Unknown option "<key>".`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local calm = assets.load('music/calm.ogg', 'sound', {stream = true})
local battle = assets.load('music/battle.ogg', 'sound', {stream = true})

audio.playMusic(calm, {volume = 0.8})
local music = audio.playMusic(battle, {fade = 2, loopStart = 8, loopEnd = 72})

-- The pause menu holds the music alone, while the interface sounds keep playing.
audio.pause(music)
audio.resume(music)
```

### audio.stopMusic(fadeOut)

Stops the music track. It fades the track out over `fadeOut` seconds, `1` by default, and `0` stops it at once.

```lua
local audio = require('haylen.audio')

audio.stopMusic()
audio.stopMusic(0)
```

### audio.music()

Returns the sound of the music track that plays or is paused, or `nil` when no music plays. A track that `audio.stop` stopped through its voice counts as over, even while it fades out.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local theme = assets.load('music/theme.ogg', 'sound', {stream = true})
if audio.music() ~= theme then
    audio.playMusic(theme)
end
```

## Buses

A bus is a group of voices with one volume, one mute, one process mode and one chain of effects. Buses form a tree under `master`, which starts with the buses `music`, `sfx`, `ui` and `ambience`. The volume of a voice is multiplied by the volume of its bus and of every bus above it. The [audio guide](../audio/audio.md#buses) explains the tree and its pause modes. The functions `preferences.capture()` and `preferences.apply()` of [`haylen.preferences`](preferences.md) save and restore the volume and mute of every bus.

### audio.createBus(name, parent)

Creates a bus under `parent`, which is `'master'` by default. A name that is empty or that another bus uses raises `An audio bus needs a non-empty name that no other bus uses, not "<name>".`. An unknown parent raises `The audio bus "<parent>" does not exist.`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

audio.createBus('footsteps', 'sfx')
audio.createBus('voices')
audio.play(assets.load('sfx/step.wav'), {bus = 'footsteps'})
```

### audio.setBusVolume(bus, volume, fade)

Changes the volume of a bus. It fades over `fade` seconds, `0` by default. A negative volume becomes `0`. A volume that is not finite raises `Audio needs a finite bus volume.`.

```lua
local audio = require('haylen.audio')

audio.setBusVolume('music', 0.5)
audio.setBusVolume('master', 0, 1.5)
```

### audio.busVolume(bus)

Returns the volume last set on a bus. During a fade it returns the volume the fade goes to.

```lua
local audio = require('haylen.audio')

print(math.floor(audio.busVolume('sfx') * 100) .. ' percent')
```

### audio.setBusMuted(bus, muted)

Mutes or unmutes a bus without changing its volume. So unmuting brings back the level the player chose.

```lua
local audio = require('haylen.audio')

audio.setBusMuted('music', true)
```

### audio.busMuted(bus)

Returns `true` while a bus is muted.

```lua
local audio = require('haylen.audio')

audio.setBusMuted('sfx', not audio.busMuted('sfx'))
```

### audio.setBusProcessMode(bus, mode)

Sets the process mode that the voices of a bus inherit: `'inherit'`, `'pausable'`, `'whenPaused'`, `'always'` or `'disabled'`. Child buses in `'inherit'` follow it too.

```lua
local audio = require('haylen.audio')
local haylen = require('haylen')

-- The music now stops with the game.
audio.setBusProcessMode('music', 'pausable')
haylen.setPaused(true)
```

### audio.busProcessMode(bus)

Returns the process mode of a bus as it was set, `'inherit'` included.

```lua
local audio = require('haylen.audio')

print(audio.busProcessMode('music'), audio.busProcessMode('sfx'), audio.busProcessMode('master')) -- always pausable inherit
```

### audio.buses()

Returns the names of every bus in alphabetical order.

```lua
local audio = require('haylen.audio')

for _, bus in ipairs(audio.buses()) do
    print(bus, audio.busVolume(bus), audio.busMuted(bus))
end
```

### audio.busStats()

Returns one table per bus, in alphabetical order, for debug overlays and tests. Each table counts the voices that play through the bus itself.

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | The name of the bus. |
| `voices` | integer | The voices on the bus. |
| `playing` | integer | The voices that play now. |
| `paused` | integer | The voices that the app, `audio.pauseAll`, an interruption or the game pause holds. |
| `processing` | boolean | Whether the process mode of the bus runs in the current pause state. |

```lua
local audio = require('haylen.audio')

for _, bus in ipairs(audio.busStats()) do
    print(bus.name, bus.voices, bus.playing, bus.paused, bus.processing)
end
```

## Effects

An effect changes the sound of a bus or of a voice, such as a filter, an echo or a room. The function `audio.newEffect` makes one of four types: [`Filter`](#filter), [`Delay`](#delay), [`Reverb`](#reverb) and [`Limiter`](#limiter). Their parameters are properties that change at any time and that tweens animate.

- Effects of one bus or one voice run in the order they were added.
- An effect processes one bus or one voice at a time.
- A voice lets the echoes and the reverb of its effects ring out after it ends. Then the effects are free again.
- An effect that moves to another bus or voice starts there without the echoes or reverb it held before.

### audio.newEffect(kind, options)

Makes an effect of `kind`. The table `options` is optional, and it sets the properties of the new effect.

| Kind | Type | Options |
| --- | --- | --- |
| `'lowpass'`, `'highpass'`, `'bandpass'`, `'notch'` | [`Filter`](#filter) | `cutoff` and `q`. |
| `'peak'`, `'lowShelf'`, `'highShelf'` | [`Filter`](#filter) | `cutoff`, `q` and `gain`. |
| `'delay'` | [`Delay`](#delay) | `time`, `maxTime`, `feedback`, `wet` and `dry`. |
| `'reverb'` | [`Reverb`](#reverb) | `roomSize`, `damping`, `width`, `wet` and `dry`. |
| `'limiter'` | [`Limiter`](#limiter) | `threshold` and `release`. |

An unknown kind raises `The audio effect must be "lowpass", "highpass", "bandpass", "notch", "peak", "lowShelf", "highShelf", "delay", "reverb" or "limiter", not "<kind>".`. An option that the kind does not take raises `Unknown option "<key>".`, so `gain` on a `'lowpass'` raises it too. A value out of range raises the error of the property it sets.

```lua
local audio = require('haylen.audio')

local muffle = audio.newEffect('lowpass', {cutoff = 800})
local bass = audio.newEffect('lowShelf', {cutoff = 200, gain = 6})
local canyon = audio.newEffect('delay', {time = 0.4, maxTime = 2, feedback = 0.5, wet = 0.5})
local hall = audio.newEffect('reverb', {roomSize = 0.8, damping = 0.2, wet = 0.3})
print(muffle.kind, bass.gain, canyon.time, hall.roomSize)
```

### audio.addEffect(voice, effect)

Adds `effect` at the end of the chain of a voice, before its bus. An effect that still processes another bus or voice, or still rings out after its voice ended, raises `The audio effect already processes another bus or voice.`. The id of a voice that ended does nothing.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local voice = audio.play(assets.load('voice/line-01.ogg'))
audio.addEffect(voice, audio.newEffect('bandpass', {cutoff = 1500, q = 1.5}))
```

### audio.removeEffect(voice, effect)

Removes `effect` from the chain of a voice. An effect that is not on the voice, and a voice that ended, do nothing.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local radio = audio.newEffect('highpass', {cutoff = 600})
local voice = audio.play(assets.load('voice/line-01.ogg'), {effects = {radio}})
audio.removeEffect(voice, radio)
```

### audio.effects(voice)

Returns the effects of a voice in order, or an empty list for a voice that ended. Each value compares equal with `==` to the effect that was added.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local echo = audio.newEffect('delay')
local voice = audio.play(assets.load('sfx/shout.wav'), {effects = {echo}})
print(#audio.effects(voice), audio.effects(voice)[1] == echo) -- 1 true
```

### audio.addBusEffect(bus, effect)

Adds `effect` at the end of the chain of a bus. It processes everything the bus mixes, before the parent bus. It raises the same errors as `audio.addEffect`.

```lua
local audio = require('haylen.audio')

audio.addBusEffect('sfx', audio.newEffect('reverb', {roomSize = 0.85, wet = 0.4}))
audio.addBusEffect('master', audio.newEffect('highShelf', {cutoff = 6000, gain = -3}))
```

### audio.removeBusEffect(bus, effect)

Removes `effect` from the chain of a bus. An effect that is not on the bus does nothing.

```lua
local audio = require('haylen.audio')

local underwater = audio.newEffect('lowpass', {cutoff = 500})
audio.addBusEffect('sfx', underwater)
audio.removeBusEffect('sfx', underwater)
```

### audio.busEffects(bus)

Returns the effects of a bus in order.

```lua
local audio = require('haylen.audio')

audio.addBusEffect('music', audio.newEffect('lowpass', {cutoff = 2000}))
for _, effect in ipairs(audio.busEffects('music')) do
    print(effect.tail, effect.attached)
end
```

## Positional audio

A positional voice has a place in the world. It fades with its distance to the listener, pans by its side of the listener and can shift its pitch while it moves. The engine updates every positional voice once per frame, after the scenes update. The [audio guide](../audio/audio.md#positional-audio) explains the fade models and the Doppler effect.

### audio.setListener(x, y)

Moves the listener, the point that hears, to a place in the world. It starts at `0, 0`. A coordinate that is not finite raises `Audio needs a finite position.`.

```lua
local audio = require('haylen.audio')
local scene = require('haylen.scene')

local player = {x = 0, y = 0}

scene.push({
    update = function(self, dt)
        player.x = player.x + 100 * dt
        audio.setListener(player.x, player.y)
    end,
})
```

### audio.listener()

Returns the place of the listener as two numbers.

```lua
local audio = require('haylen.audio')

local x, y = audio.listener()
print(x, y)
```

### audio.followCamera(camera)

Moves the listener to a [`Camera`](graphics2d.md#camera) on every frame, at its position plus its offset and without its shake. The engine keeps the camera alive while the listener follows it. The call `audio.followCamera(nil)` stops following and leaves the listener where it is. A value that is not a camera raises a bad argument error with `haylen.Camera expected`.

```lua
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
        audio.followCamera(self.camera)
    end,
    update = function(self, dt)
        self.camera:follow(400, 300, dt)
    end,
    exit = function(self)
        audio.followCamera(nil)
    end,
})
```

### audio.setSpatialization(settings)

Changes how positional voices sound. It changes the keys that `settings` names and keeps the others.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `model` | string | `'linear'` | How the volume falls with distance: `'linear'`, `'inverse'` or `'exponential'`. |
| `minDistance` | number | `100` | Within this distance a voice plays at full volume. |
| `maxDistance` | number | `1500` | The distance where the fade stops. With `'linear'` and a `rolloff` of `1`, a voice is silent there. |
| `rolloff` | number | `1` | How fast the volume falls between the two distances. |
| `panDistance` | number | `1500` | A voice this far to the side of the listener, or farther, plays only on that side. |
| `doppler` | number | `0` | How strongly motion shifts the pitch. The value `0` turns the Doppler effect off. The pitch stays within two octaves up or down. |
| `speedOfSound` | number | `3430` | The speed of sound in world units per second. The default is 343 meters per second at 10 units per meter. |

These errors can happen:

- An unknown key raises `Unknown option "<key>".`.
- A value that is not finite raises `Audio needs a finite spatialization setting.`.
- Distances outside `0 <= minDistance < maxDistance` raise `Audio attenuation needs 0 <= minimum distance < maximum distance.`.
- A `minDistance` of `0` with `'inverse'` or `'exponential'` raises `The inverse and exponential audio models need a minimum distance above 0.`.
- A negative `rolloff` or `doppler` raises `The audio rolloff and Doppler factor cannot be negative.`.
- A `panDistance` or `speedOfSound` of `0` or less raises `The audio pan distance and speed of sound must be positive.`.

```lua
local audio = require('haylen.audio')

audio.setSpatialization({model = 'inverse', minDistance = 50, maxDistance = 1000, rolloff = 1.5})
audio.setSpatialization({doppler = 1, speedOfSound = 2000})
```

### audio.spatialization()

Returns every key of the spatialization in one table.

```lua
local audio = require('haylen.audio')

local settings = audio.spatialization()
print(settings.model, settings.minDistance, settings.maxDistance, settings.doppler)
```

## Output

The output is the audio device that plays the mix. These functions tell its format and its state. The [audio guide](../audio/audio.md#without-an-audio-output) explains what happens when there is no output.

### audio.sampleRate()

Returns the sample rate of the output in hertz, `48000` by default.

```lua
local audio = require('haylen.audio')

print(audio.sampleRate()) -- 48000
```

### audio.channels()

Returns the number of output channels, `2` by default.

```lua
local audio = require('haylen.audio')

print(audio.channels()) -- 2
```

### audio.hasDevice()

Returns `true` when the mixer plays in real time for an audio device. That is the case in the player and in every app, even while the device is unavailable. The headless engine of tests and tools mixes without a device and returns `false`.

```lua
local audio = require('haylen.audio')

if not audio.hasDevice() then
    print('This is the headless engine, so nothing is heard.')
end
```

### audio.outputAvailable()

Returns `true` while the mix reaches an audio device. It returns `false` in these cases:

- While the system still opens the device, which `audio.outputOpening()` tells.
- While the system refuses the device, such as on a computer without one or while another app holds the iOS audio session.
- On a web page served over plain `http`, where browsers offer no `AudioWorklet`.
- In the headless engine.

The app runs all the same. Voices, music, fades and positional audio go on in real time without sound. The log writes one warning with the reason, and the engine tries to open the output again whenever the app becomes active or an interruption ends.

```lua
local audio = require('haylen.audio')

if audio.hasDevice() and not audio.outputAvailable() then
    print('The sound is off until this device offers an audio output.')
end
```

### audio.outputState()

Returns what the output does now.

| State | Meaning |
| --- | --- |
| `'running'` | The output plays. |
| `'locked'` | A browser waits for the first tap, click or key on the page before it plays, so an app can show a hint to turn the sound on. The event `audioUnlocked` tells when it plays. |
| `'suspended'` | The app is in the background, or an interruption holds the sound. |
| `'unavailable'` | There is no output, as [`audio.outputAvailable`](#audiooutputavailable) tells. |

```lua
local audio = require('haylen.audio')
local events = require('haylen.events')

if audio.outputState() == 'locked' then
    print('Tap to turn the sound on.')
end
events.on('audioUnlocked', function()
    print('The sound is on.')
end)
```

### audio.outputLatency()

Returns the seconds of sound that the buffers of the device hold between the mix and the speaker, or `0` without an output. The key `audio.latency` of `app.json` trades this delay for power:

- `'low'` asks for short buffers and, on Android, the low latency mode of AAudio.
- `'balanced'`, the default, keeps the default of the platform.
- `'powerSaving'` asks for long buffers, which wake the processor less often.

Browsers keep the output of the page 50 milliseconds ahead of the speaker, whatever the key says.

```lua
local audio = require('haylen.audio')

print(string.format('The output latency is %.1f milliseconds.', audio.outputLatency() * 1000))
```

### audio.outputOpening()

Returns `true` while the system opens the audio device. The engine opens it on a thread of its own when the app starts and whenever it opens it again. The app plays sound from the first frame after the device opened, so a slow device never holds the frames back. A device that does not answer within 3 seconds of frames makes the log write one warning, and the log tells again when the device opens. Browsers open the output of the page at once, so it is never opening there.

```lua
local audio = require('haylen.audio')
local events = require('haylen.events')

events.on('appActive', function()
    if audio.outputOpening() then
        print('The sound starts as soon as the device answers.')
    end
end)
```

### audio.outputMuted()

Returns `true` while the engine silences the output. It does so while the [cover](../core/lifecycle.md#covered-by-native-ui) of a plugin lasts, and while the app is not active when the lifecycle option `muteOnFocusLoss` is on. This silence comes after every bus. So `audio.busMuted` still reports the mutes the app set, which the app can change and save meanwhile.

```lua
local audio = require('haylen.audio')

print(audio.outputMuted())
```

### audio.restartOutput()

Opens the audio device again. The engine does this itself when the system resets its audio services, such as the media services of iOS. The voices go on and play from the first frame after the device opened. A device that is still opening goes on opening instead.

```lua
local audio = require('haylen.audio')

audio.restartOutput()
```

### audio.interrupted()

Returns `true` while the system holds the audio, such as during a phone call, an alarm or Siri, or while another Android app has the audio focus. The engine pauses every voice meanwhile and publishes the [events](#events). An interruption that ends while the system still refuses the audio ends all the same, and the app goes on without an output, as `audio.outputAvailable()` tells.

```lua
local audio = require('haylen.audio')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if audio.interrupted() ~= self.wasInterrupted then
            self.wasInterrupted = audio.interrupted()
            print(self.wasInterrupted and 'The sound comes back when the call ends.' or 'The sound is back.')
        end
    end,
})
```

## Sound

A `Sound`, named `haylen.Sound` in messages, is the audio data of a file. The functions `assets.load` and `assets.loadAsync` of [`haylen.assets`](assets.md) return one for a sound file, and [`audio.newSound`](#audionewsoundbytes-options) makes one from bytes. Two handles of the same loaded sound compare equal with `==`. Every property is read-only, and reading a member that a sound does not have raises `The type "haylen.Sound" has no member "<name>".`.

| Property | Type | Meaning |
| --- | --- | --- |
| `duration` | number | The length in seconds. |
| `channels` | integer | The channels of the file, `1` for mono and `2` for stereo. |
| `sampleRate` | integer | The sample rate of the file in hertz. |
| `frameCount` | integer | The length in frames, one sample for every channel. |
| `streamed` | boolean | `true` when the sound streams, from the `stream` option of assets or of `audio.newSound`. |

```lua
local assets = require('haylen.assets')

local hit = assets.load('sfx/hit.wav')
print(hit.duration, hit.channels, hit.sampleRate, hit.frameCount, hit.streamed)
```

## Filter

A `Filter`, named `haylen.Filter`, is a second-order filter that `audio.newEffect` makes for the kinds `'lowpass'`, `'highpass'`, `'bandpass'`, `'notch'`, `'peak'`, `'lowShelf'` and `'highShelf'`. Its parameters are properties that change at any time and that tweens animate.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | the kind | The kind it was made with. Read-only. |
| `cutoff` | number | `1000` | The corner or center frequency in hertz. It must be above `0`. A cutoff at or above half the sample rate acts just below it. |
| `q` | number | `0.7071` | How sharp the filter is. It sets the resonance of the pass and shelf kinds and the width of the band of the others, where larger is narrower. It must be above `0`. |
| `gain` | number | `0` | The decibels that `'peak'`, `'lowShelf'` and `'highShelf'` add or remove, from `-96` to `96`. |
| `attached` | boolean | `false` | Whether it processes a bus or a voice now. Read-only. |
| `tail` | number | `0` | The seconds it keeps sounding after its input falls silent, always `0` for filters. Read-only. |

A cutoff of `0` or less raises `A filter cutoff must be a positive frequency.`. A `q` of `0` or less raises `A filter "q" must be positive.`. A gain out of range raises `A filter gain must be between -96 and 96 decibels.`. Setting the gain of another kind raises `Only peak and shelf filters have a gain.`.

```lua
local audio = require('haylen.audio')
local tween = require('haylen.tween')

local sweep = audio.newEffect('bandpass', {cutoff = 300, q = 4})
audio.addBusEffect('music', sweep)
tween.to(sweep, 2, {cutoff = 3000}, {repeatCount = -1, loopMode = 'yoyo'})
print(sweep.kind, sweep.attached)
```

## Delay

A `Delay`, named `haylen.Delay`, is an echo that `audio.newEffect('delay')` makes. It repeats its input after `time` seconds, and each repeat is `feedback` times as loud as the one before.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `time` | number | `0.25` | The seconds between the input and each repeat, from `0` to `maxTime`. A change glides to the new time, which bends the pitch of the repeats. |
| `maxTime` | number | `1` | The longest time, above `0` and at most `10`. It is set when the delay is made. Read-only. |
| `feedback` | number | `0.4` | The level of each repeat against the one before, at least `0` and below `1`. |
| `wet` | number | `0.5` | The level of the repeats, `0` or more. |
| `dry` | number | `1` | The level of the input, `0` or more. |
| `attached` | boolean | `false` | Whether it processes a bus or a voice now. Read-only. |
| `tail` | number | none | The seconds until the repeats fall 60 decibels below the first one. Read-only. |

A time out of range raises `A delay time must be between 0 and the largest time of the delay.`. A feedback out of range raises `A delay feedback must be at least 0 and below 1.`. A negative level raises `A delay wet level must be 0 or more.` or `A delay dry level must be 0 or more.`. A bad `maxTime` raises `A delay needs a largest time above 0 and up to 10 seconds.`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local echo = audio.newEffect('delay', {time = 0.3, maxTime = 1, feedback = 0.4, wet = 0.5})
audio.play(assets.load('sfx/shout.wav'), {effects = {echo}})
echo.time = 0.5
echo.feedback = 0.6
print(echo.maxTime, echo.tail)
```

## Reverb

A `Reverb`, named `haylen.Reverb`, places sound in a room. The function `audio.newEffect('reverb')` makes one with the Freeverb design. Every parameter lies between `0` and `1`, and a value outside raises `A reverb <parameter> must be between 0 and 1.`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `roomSize` | number | `0.5` | The size of the room, which sets how long the reverb lasts. |
| `damping` | number | `0.5` | How much the reverb softens its high frequencies. |
| `width` | number | `1` | The stereo width of the reverb, from mono at `0` to fully spread at `1`. |
| `wet` | number | `0.33` | The level of the reverb. |
| `dry` | number | `1` | The level of the input. |
| `attached` | boolean | `false` | Whether it processes a bus or a voice now. Read-only. |
| `tail` | number | none | The seconds until the reverb falls 60 decibels. Read-only. |

```lua
local audio = require('haylen.audio')
local tween = require('haylen.tween')

local room = audio.newEffect('reverb', {roomSize = 0.3, wet = 0.2})
audio.addBusEffect('sfx', room)

-- The player walks from a corridor into a hall.
tween.to(room, 1.5, {roomSize = 0.9, wet = 0.45, damping = 0.2, width = 1, dry = 0.9})
print(room.tail)
```

## Limiter

A `Limiter`, named `haylen.Limiter`, keeps every sample under its `threshold`, so thirty explosions at once never clip and crackle. Its gain drops at once to fit a peak, then comes back toward `1` over `release` seconds. The function `audio.newEffect('limiter')` makes one. The engine also puts one with the defaults on the `master` bus of every app, unless the key `audio.limiter` of `app.json` is `false`, and `audio.busEffects('master')[1]` returns it.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `threshold` | number | `0.9` | The highest level of a sample, above `0` and at most `1`. |
| `release` | number | `0.1` | The seconds in which the gain recovers after a peak, from `0` to `10`. |
| `reduction` | number | `0` | How far the gain dropped in the last block, from `0` to `1`, for meters. Read-only. |
| `attached` | boolean | `false` | Whether it processes a bus or a voice now. Read-only. |
| `tail` | number | `0` | Always `0`, since a limiter keeps nothing sounding. Read-only. |

A threshold out of range raises `A limiter threshold must be above 0 and at most 1.`. A release out of range raises `A limiter release must be from 0 to 10 seconds.`.

```lua
local audio = require('haylen.audio')

local master = audio.busEffects('master')[1]
master.threshold = 0.8
local sfxLimiter = audio.newEffect('limiter', {threshold = 0.7, release = 0.05})
audio.addBusEffect('sfx', sfxLimiter)
print(sfxLimiter.reduction, sfxLimiter.attached, sfxLimiter.tail)
```

## Events

The engine publishes these events on [`haylen.events`](events.md). Their listeners receive no arguments.

| Event | When |
| --- | --- |
| `audioInterrupted` | The system took the audio, for a phone call, an alarm, Siri or another Android app with the audio focus. Every voice is paused, and `audio.interrupted()` returns `true`. |
| `audioResumed` | The interruption ended while the app is active, or the app became active after it ended. The voices resume. The audio session is active again unless the system still refuses it, which `audio.outputAvailable()` tells. |
| `audioRouteChanged` | The output moved to another device, such as headphones that were unplugged. |
| `audioUnlocked` | A browser let the output play after the first tap, click or key on the page. |

```lua
local events = require('haylen.events')
local haylen = require('haylen')

events.on('audioInterrupted', function()
    haylen.setPaused(true)
end)

events.on('audioRouteChanged', function()
    print('The output device changed.')
end)
```

## Errors

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | An options table has a key that the function does not take. |
| `Audio data is not a supported WAV, FLAC, MP3 or Ogg Vorbis file.` | The function `audio.newSound` got bytes that are not a sound file. |
| `Audio data contains no samples.` | The function `audio.newSound` got a file or raw samples without a sample. |
| `The format of raw samples is "float32" or "int16", not "<format>".` | The option `format` of `audio.newSound` names another format. |
| `Raw audio needs a sample rate and channels.` | The function `audio.newSound` got raw samples without `sampleRate` or `channels`. |
| `Samples of this format take <n> bytes each, which <m> bytes do not fill.` | The bytes of raw samples do not fill whole samples. |
| `Raw audio samples come in whole frames, one sample for every channel.` | The raw samples do not fill whole frames. |
| `Only raw samples take a sample rate and channels, together with their format.` | The function `audio.newSound` got `sampleRate` or `channels` for a file. |
| `Raw samples are decoded already, so they cannot stream.` | The function `audio.newSound` got raw samples with `stream = true`. |
| `The audio bus "<name>" does not exist.` | A call names a bus that does not exist. |
| `An audio bus needs a non-empty name that no other bus uses, not "<name>".` | The function `audio.createBus` got an empty name or the name of another bus. |
| `The priority of a voice runs from 0 to 255.` | The function `audio.play` got a `priority` out of range. |
| `A pitch variation must be at least 0 and smaller than the pitch.` | The function `audio.play` got a bad `pitchVariation`. |
| `Audio needs a finite volume.` | The function `audio.play` or `audio.setVolume` got a volume that is not finite, such as `0 / 0` or `math.huge`. |
| `Audio needs a finite pitch above 0.` | The function `audio.play` or `audio.setPitch` got a pitch of `0` or less, or one that is not finite. |
| `Audio needs a finite pan.` | The function `audio.play` or `audio.setPan` got a pan that is not finite. |
| `Audio needs a finite start time.` | The function `audio.play` or `audio.playMusic` got a `startAt` that is not finite. |
| `Audio needs a finite position.` | The function `audio.play`, `audio.setPosition` or `audio.setListener` got a coordinate that is not finite. |
| `Audio needs a finite cursor.` | The function `audio.setCursor` got a time that is not finite. |
| `Audio needs a finite bus volume.` | The function `audio.setBusVolume` got a volume that is not finite. |
| `Audio needs a fade from 0 to 86400 seconds.` | A fade is below `0`, longer than a day or not a number: the `fadeIn` of `audio.play`, the `fade` of `audio.playMusic`, or the fade of `audio.stop`, `audio.stopAll`, `audio.stopMusic` or `audio.setBusVolume`. |
| `A loop needs 0 <= loopStart < loopEnd <= the length of the sound.` | The function `audio.play` or `audio.playMusic` got loop points outside the sound or in the wrong order. |
| `A loop needs to span at least one frame of the sound.` | The loop points lie closer than one frame of the sound. |
| `The voice of a live audio stream has no cursor to move.` | The function `audio.setCursor` got the voice of an audio stream. |
| `The "steal" of a sound limit must be "oldest", "quietest" or "none".` | The function `audio.setLimit` got another `steal`. |
| `The cooldown of a sound must be a finite number of seconds from 0.` | The function `audio.setLimit` got a negative cooldown, or one that is not finite. |
| `Audio needs a finite spatialization setting.` | The function `audio.setSpatialization` got a value that is not finite. |
| `Audio attenuation needs 0 <= minimum distance < maximum distance.` | The function `audio.setSpatialization` got bad distances. |
| `The inverse and exponential audio models need a minimum distance above 0.` | The function `audio.setSpatialization` picked one of these models with a `minDistance` of `0`. |
| `The audio rolloff and Doppler factor cannot be negative.` | The function `audio.setSpatialization` got a negative `rolloff` or `doppler`. |
| `The audio pan distance and speed of sound must be positive.` | The function `audio.setSpatialization` got a `panDistance` or `speedOfSound` of `0` or less. |
| `The audio effect must be "lowpass", "highpass", "bandpass", "notch", "peak", "lowShelf", "highShelf", "delay", "reverb" or "limiter", not "<kind>".` | The function `audio.newEffect` got an unknown kind. |
| `The audio effect already processes another bus or voice.` | An effect was added while it is still on a bus or a voice, or still rings out after its voice ended. |
| `The option "effects" must be a list of audio effects.` | The option `effects` of `audio.play` is not a list of effects. |
| `A filter cutoff must be a positive frequency.` | A filter got a cutoff of `0` or less. |
| `A filter "q" must be positive.` | A filter got a `q` of `0` or less. |
| `A filter gain must be between -96 and 96 decibels.` | A peak or shelf filter got a gain out of range, or one that is not a number. |
| `Only peak and shelf filters have a gain.` | Another filter got a gain. |
| `A delay time must be between 0 and the largest time of the delay.` | A delay got a `time` out of range. |
| `A delay feedback must be at least 0 and below 1.` | A delay got a `feedback` out of range. |
| `A delay wet level must be 0 or more.` | A delay got a negative `wet`. |
| `A delay dry level must be 0 or more.` | A delay got a negative `dry`. |
| `A delay needs a largest time above 0 and up to 10 seconds.` | The function `audio.newEffect` got a bad `maxTime`. |
| `A reverb <parameter> must be between 0 and 1.` | A reverb got a parameter out of range. |
| `A limiter threshold must be above 0 and at most 1.` | A limiter got a threshold out of range. |
| `A limiter release must be from 0 to 10 seconds.` | A limiter got a release out of range. |
| `audio effect expected` | A function that takes an effect got another value. It comes inside a bad argument error. |
| `haylen.Camera expected` | The function `audio.followCamera` got a value that is not a camera. It comes inside a bad argument error. |
| `expected a non-negative integer` | A voice id or the seed of `audio.seedVariation` is negative. It comes inside a bad argument error. |
| `The type "haylen.Sound" has no member "<name>".` | The code read a member that a sound does not have. |
