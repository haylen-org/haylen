# haylen.audio

Sound effects, music and mixing. Use it to play loaded sounds as voices, crossfade music tracks, group sounds into buses for volume sliders and effects, let the game pause stop the right sounds, and place sounds in the world so they fade, pan and shift with distance and motion. The [audio guide](../audio.md) explains the concepts behind these functions.

```lua
local audio = require('haylen.audio')
```

## Sounds and voices

Sounds are assets loaded through [`haylen.assets`](assets.md) from WAV, Ogg Vorbis, MP3 and FLAC files. A sound is decoded into memory by default, which suits short effects. The `stream` option keeps the encoded file in memory and decodes it while it plays, which suits long music.

Every call to `audio.play()` starts a new voice and returns its id, an integer, and `audio.playMusic()` returns the id of the voice of the music track. The same sound can play on many voices at once, and each voice has its own volume, pitch, pan, position and playback cursor. Calls with the id of a voice that already finished do nothing, so an app never has to check a voice before stopping it. At most 128 voices play at once, and a new voice stops the oldest voice that is not music when the limit is reached.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local jump = assets.load('sfx/jump.wav')
local theme = assets.load('music/theme.ogg', 'sound', {stream = true})

local voice = audio.play(jump)
audio.playMusic(theme)
```

## Buses

Every voice plays through a bus. Buses form a tree whose root, `master`, feeds the output. The buses `music`, `sfx`, `ui` and `ambience` exist from the start as children of `master`. Voices use `sfx` and music uses `music` unless the options name another bus. The volume of a voice is multiplied by the volume of its bus and of every bus above it.

Every bus has a process mode that decides whether its voices play while the game is paused, as [`haylen.setPaused`](haylen.md) pauses it. The buses `music` and `ui` start as `'always'`, `sfx` and `ambience` as `'pausable'`, and `master` and custom buses as `'inherit'`, which takes the mode of the parent bus and counts as `'pausable'` at `master`. A voice paused by the game keeps its cursor and resumes where it stopped.

Sounds also come from bytes in memory with [`audio.newSound`](#audionewsoundbytes-options), such as the audio a plugin returns from native code, and the audio streams of plugins play as voices, as [`haylen.platform`](platform.md#audio-streams) describes.

## Functions

### audio.newSound(bytes, options)

Creates a `haylen.Sound` from `bytes`, a string with the contents of a WAV, Ogg Vorbis, MP3 or FLAC file, decoded into memory unless `options.stream` is `true`, which keeps the file and decodes it while a voice plays it. With `options.format` the string holds raw samples instead, interleaved by channel in the byte order of the device, which is little-endian on every platform the engine runs on:

| Option | Type | Meaning |
| --- | --- | --- |
| `stream` | boolean | Keeps a file encoded and decodes it while it plays, like the `stream` option of assets. It is `false` by default, and raw samples, which are decoded already, raise `Raw samples are decoded already, so they cannot stream.`. |
| `format` | string | Either `'float32'` for 32-bit floats from -1 to 1, or `'int16'` for 16-bit integers, which become floats. Any other format raises `The format of raw samples is "float32" or "int16", not "<format>".`. |
| `sampleRate` | integer | The sample rate of raw samples in hertz, which they need. |
| `channels` | integer | The channels of raw samples, which they need. |

Bytes that are no sound file raise `Audio data is not a supported WAV, FLAC, MP3 or Ogg Vorbis file.`, raw samples without a sample rate or channels raise `Raw audio needs a sample rate and channels.`, bytes that do not fill whole samples raise `Samples of this format take <n> bytes each, which <m> bytes do not fill.`, samples that do not fill whole frames raise `Raw audio samples come in whole frames, one sample for every channel.`, and a sample rate or channels given for a file raise `Only raw samples take a sample rate and channels, together with their format.`.

```lua
local async = require('async')
local audio = require('haylen.audio')
local platform = require('haylen.platform')

-- A tone of a quarter second made in Lua, as 16-bit samples at 22050 Hz.
local samples = {}
for index = 1, 5512 do
    samples[index] = string.pack('<h', math.floor(math.sin(index * 2 * math.pi * 440 / 22050) * 8000))
end
local beep = audio.newSound(table.concat(samples), {format = 'int16', sampleRate = 22050, channels = 1})
audio.play(beep)

async.spawn(function()
    -- The native part answers with a recording as the bytes of a WAV file.
    local recording = platform.call('recorder.lastRecording'):await()
    if recording then
        audio.play(audio.newSound(recording.wav))
    end
end)
```

### audio.play(sound, options)

Starts a voice for the sound and returns its id. The options table is optional, and unknown keys raise `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `bus` | string | `'sfx'` | Bus the voice plays through. |
| `volume` | number | `1` | Volume of the voice, where 1 is the original level. |
| `pitch` | number | `1` | Playback speed and pitch, where 2 is one octave up. |
| `pitchVariation` | number | `0` | Picks the pitch of this play at random between `pitch - pitchVariation` and `pitch + pitchVariation`, so repeated sounds never sound exactly alike. It must be at least 0 and smaller than `pitch`. |
| `pan` | number | `0` | Stereo position from -1 (left) to 1 (right). |
| `loop` | boolean | `false` | Repeats the sound until the voice is stopped. |
| `fadeIn` | number | `0` | Seconds to fade in from silence. |
| `startAt` | number | `0` | Seconds into the sound where playback starts. |
| `x`, `y` | number | none | World position of the voice. Giving either one makes the voice positional, and the missing one defaults to 0. |
| `processMode` | string | `'inherit'` | One of `'inherit'`, `'pausable'`, `'whenPaused'`, `'always'` or `'disabled'`. The mode `'inherit'` takes the mode of the bus. |
| `effects` | table | none | A list of effects from `audio.newEffect` that process the voice in order before its bus. |

An unknown bus raises `The audio bus "<name>" does not exist.`, a bad variation raises `A pitch variation must be at least 0 and smaller than the pitch.`, and an `effects` value that is not a list of effects raises `The option "effects" must be a list of audio effects.`. Numbers must be finite, so `0 / 0` or `math.huge` for `volume`, `pan`, `fadeIn`, `startAt`, `x` or `y` raise `Audio needs a finite volume.`, `Audio needs a finite pan.`, `Audio needs a finite fade-in.`, `Audio needs a finite start time.` or `Audio needs a finite position.`, and a `pitch` that is not a finite number above 0 raises `Audio needs a finite pitch above 0.`. A call that raises an error stops no voice, even when every voice is busy.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local step = assets.load('sfx/step.wav')
local fire = assets.load('sfx/fire.ogg')
local click = assets.load('ui/click.wav')

audio.play(step, {volume = 0.6, pitchVariation = 0.1})
local crackle = audio.play(fire, {bus = 'ambience', loop = true, fadeIn = 2, x = 400, y = 120})
audio.play(click, {bus = 'ui', pan = -0.3, startAt = 0.02})
audio.play(click, {bus = 'ui', processMode = 'always', effects = {audio.newEffect('highpass', {cutoff = 400})}})
```

### audio.stop(voice, fadeOut)

Stops a voice, fading it out over `fadeOut` seconds when given. The fade defaults to 0.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local engine = audio.play(assets.load('sfx/engine.wav'), {loop = true})
audio.stop(engine, 0.5)
```

### audio.pause(voice)

Pauses a voice. A paused voice keeps its cursor and still counts as active.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local rain = audio.play(assets.load('sfx/rain.ogg'), {bus = 'ambience', loop = true})
audio.pause(rain)
```

### audio.resume(voice)

Resumes a paused voice from where it stopped.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local rain = audio.play(assets.load('sfx/rain.ogg'), {bus = 'ambience', loop = true})
audio.pause(rain)
audio.resume(rain)
```

### audio.paused(voice)

Returns `true` while the app holds the voice paused with `audio.pause`. The game pause and `audio.pauseAll` do not count, since they hold the voice for reasons of their own. A finished voice returns `false`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local radio = audio.play(assets.load('sfx/radio.ogg'), {loop = true})
audio.pause(radio)
audio.pauseAll()
audio.resumeAll()
print(audio.paused(radio))
```

### audio.processMode(voice)

Returns the process mode the voice was played with, `'inherit'` unless `audio.play` named another. A finished voice returns `'inherit'`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local tick = audio.play(assets.load('sfx/tick.wav'), {loop = true, processMode = 'always'})
print(audio.processMode(tick))
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

Changes the pitch and playback speed of a voice. A pitch that is not a finite number above 0 raises `Audio needs a finite pitch above 0.`.

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

Returns the current pitch of a voice, including the random part picked by `pitchVariation`. A finished voice returns 0.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local voice = audio.play(assets.load('sfx/coin.wav'), {pitch = 1.2, pitchVariation = 0.1})
print(audio.pitch(voice))
```

### audio.seedVariation(seed)

Seeds the random generator behind `pitchVariation` with a non-negative integer, so the same seed picks the same pitches again. It suits replays and tests. A negative seed raises a bad argument error with `expected a non-negative integer`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local step = assets.load('sfx/step.wav')
audio.seedVariation(42)
for index = 1, 3 do
    print(audio.pitch(audio.play(step, {pitchVariation = 0.1})))
end
```

### audio.setPan(voice, pan)

Changes the stereo position of a voice from -1 (left) to 1 (right). Positional voices add their distance pan to this value. A pan that is not finite raises `Audio needs a finite pan.`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local voice = audio.play(assets.load('sfx/bird.wav'))
audio.setPan(voice, 0.8)
```

### audio.setPosition(voice, x, y)

Moves a voice to a world position and makes it positional. The engine refreshes the volume and pan of positional voices every frame, as described in [Positional audio](#positional-audio). A coordinate that is not finite raises `Audio needs a finite position.`.

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

### audio.active(voice)

Returns `true` while the voice plays or is paused, and `false` once it finished or was stopped, even while `audio.stop` still fades it out.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local scene = require('haylen.scene')

local intro = audio.play(assets.load('sfx/intro.wav'))

scene.push({
    update = function(self, dt)
        if not audio.active(intro) then
            print('intro finished')
        end
    end,
})
```

### audio.cursor(voice)

Returns the playback position of a voice in seconds, or 0 for a finished voice.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local speech = audio.play(assets.load('voice/line_01.ogg'))
print(audio.cursor(speech))
```

### audio.stopAll(fadeOut)

Stops every voice, including music, fading them out over `fadeOut` seconds when given. The fade defaults to 0.

```lua
local audio = require('haylen.audio')

audio.stopAll(0.3)
```

### audio.voiceCount()

Returns how many voices exist. Finished voices are released once per frame, so a voice that just ended still counts until the end of the frame.

```lua
local audio = require('haylen.audio')

print(audio.voiceCount())
```

### audio.pauseAll()

Pauses every voice that exists now, music included, such as while a cutscene or a dialog takes over. Voices started afterwards play as usual.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

audio.play(assets.load('sfx/rain.ogg'), {bus = 'ambience', loop = true})
audio.pauseAll()
audio.play(assets.load('ui/open.wav'), {bus = 'ui'})
```

### audio.resumeAll()

Resumes the voices that `audio.pauseAll` paused. A voice that the app paused with `audio.pause`, that the game pause holds or that an interruption holds stays paused.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local radio = audio.play(assets.load('sfx/radio.ogg'), {loop = true})
local rain = audio.play(assets.load('sfx/rain.ogg'), {bus = 'ambience', loop = true})
audio.pause(radio)
audio.pauseAll()
audio.resumeAll()
print(audio.paused(radio), audio.paused(rain))
```

### audio.interrupted()

Returns `true` while the system holds the audio, such as during a phone call, an alarm or Siri, or while another Android app has the audio focus. The engine pauses every voice for the interruption and publishes the [audio events](#events). An interruption that ends while the system still refuses the audio ends all the same, and the app goes on without an output, as `audio.outputAvailable()` reports.

```lua
local audio = require('haylen.audio')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        if audio.interrupted() then
            print('the audio comes back when the call ends')
        end
    end,
})
```

## Effects

Effects process the sound of a bus or of a voice, in the order they were added. The function `audio.newEffect` creates them as [`Filter`](#filter), [`Delay`](#delay) and [`Reverb`](#reverb) values whose parameters are properties, which change at any time and which tweens animate. An effect processes one bus or one voice at a time, and a voice lets its effects ring out after it ends before they are free again. An effect that moves to another bus or voice starts there without the echoes or reverb it held before.

### audio.newEffect(kind, options)

Creates an effect. The argument `kind` is one of `'lowpass'`, `'highpass'`, `'bandpass'`, `'notch'`, `'peak'`, `'lowShelf'`, `'highShelf'`, `'delay'` and `'reverb'`, and the options table is optional. An unknown kind raises `The audio effect must be "lowpass", "highpass", "bandpass", "notch", "peak", "lowShelf", "highShelf", "delay" or "reverb", not "<kind>".`, unknown options raise `Unknown option "<key>".`, and values out of range raise the errors of the property they set.

| Kind | Options |
| --- | --- |
| `'lowpass'`, `'highpass'`, `'bandpass'`, `'notch'` | `cutoff` in hertz (1000) and `q` (0.7071). |
| `'peak'`, `'lowShelf'`, `'highShelf'` | `cutoff` (1000), `q` (0.7071) and `gain` in decibels (0). |
| `'delay'` | `time` in seconds (0.25), `maxTime` in seconds (1, at most 10), `feedback` (0.4), `wet` (0.5) and `dry` (1). |
| `'reverb'` | `roomSize` (0.5), `damping` (0.5), `width` (1), `wet` (0.33) and `dry` (1). |

```lua
local audio = require('haylen.audio')

local muffle = audio.newEffect('lowpass', {cutoff = 800})
local bass = audio.newEffect('lowShelf', {cutoff = 200, gain = 6})
local canyon = audio.newEffect('delay', {time = 0.4, maxTime = 2, feedback = 0.5, wet = 0.5})
local hall = audio.newEffect('reverb', {roomSize = 0.8, damping = 0.2, wet = 0.3})
print(muffle.kind, bass.gain, canyon.time, hall.roomSize)
```

### audio.addEffect(voice, effect)

Adds an effect at the end of the chain of a voice, before its bus. An effect that already processes another bus or voice raises `The audio effect already processes another bus or voice.`. A finished voice does nothing.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local voice = audio.play(assets.load('voice/line_01.ogg'))
audio.addEffect(voice, audio.newEffect('bandpass', {cutoff = 1500, q = 1.5}))
```

### audio.removeEffect(voice, effect)

Removes an effect from the chain of a voice. An effect that is not on the voice, and a finished voice, do nothing.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local radio = audio.newEffect('highpass', {cutoff = 600})
local voice = audio.play(assets.load('voice/line_01.ogg'), {effects = {radio}})
audio.removeEffect(voice, radio)
```

### audio.effects(voice)

Returns the effects of a voice in order, or an empty list for a finished voice. The values compare equal with `==` to the effects that were added.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local echo = audio.newEffect('delay')
local voice = audio.play(assets.load('sfx/shout.wav'), {effects = {echo}})
print(#audio.effects(voice), audio.effects(voice)[1] == echo)
```

### audio.addBusEffect(bus, effect)

Adds an effect at the end of the chain of a bus, where it processes everything the bus mixes before the parent bus.

```lua
local audio = require('haylen.audio')

audio.addBusEffect('sfx', audio.newEffect('reverb', {roomSize = 0.85, wet = 0.4}))
audio.addBusEffect('master', audio.newEffect('highShelf', {cutoff = 6000, gain = -3}))
```

### audio.removeBusEffect(bus, effect)

Removes an effect from the chain of a bus. An effect that is not on the bus does nothing.

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
    print(effect.kind, effect.cutoff)
end
```

## Music

One music track plays at a time on its own voice. When the voice limit is reached, other voices stop before the music voice.

### audio.playMusic(sound, options)

Starts a music track, crossfades from the previous one and returns the voice id of the track. The voice works with every voice function, so `audio.pause` and `audio.resume` pause the music alone, and `audio.setVolume`, `audio.addEffect` and `audio.stop` reach the track. When the requested track is already playing or paused, it keeps its voice, which the call returns, and only its volume changes. A paused track that the next track replaces stops without sounding again, and the next track plays. A track that cannot play, such as one for an unknown bus, raises the errors of [`audio.play`](#audioplaysound-options) and leaves the current track playing. The options table is optional, and unknown keys raise `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `bus` | string | `'music'` | Bus the track plays through. |
| `volume` | number | `1` | Volume of the track. |
| `fade` | number | `1` | Seconds of the crossfade, used to fade the old track out and the new one in. |
| `loop` | boolean | `true` | Repeats the track. |

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local calm = assets.load('music/calm.ogg', 'sound', {stream = true})
local battle = assets.load('music/battle.ogg', 'sound', {stream = true})

audio.playMusic(calm, {volume = 0.8})
local music = audio.playMusic(battle, {fade = 2})

-- The pause menu holds the music alone, while the interface sounds keep playing.
audio.pause(music)
audio.resume(music)
```

### audio.stopMusic(fadeOut)

Stops the music track, fading it out over `fadeOut` seconds. The fade defaults to 1 second.

```lua
local audio = require('haylen.audio')

audio.stopMusic()
audio.stopMusic(0)
```

### audio.music()

Returns the sound of the music track that is playing or paused, or `nil` when no music plays. A track stopped through its voice with `audio.stop` counts as over while it fades out.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local theme = assets.load('music/theme.ogg', 'sound', {stream = true})
if audio.music() ~= theme then
    audio.playMusic(theme)
end
```

## Bus functions

### audio.createBus(name, parent)

Creates a bus under `parent`, which defaults to `'master'`. A name that is empty or already used raises `An audio bus needs a non-empty name that no other bus uses, not "<name>".`, and an unknown parent raises `The audio bus "<parent>" does not exist.`

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

audio.createBus('footsteps', 'sfx')
audio.createBus('voices')
audio.play(assets.load('sfx/step.wav'), {bus = 'footsteps'})
```

### audio.setBusVolume(bus, volume, fade)

Changes the volume of a bus, fading over `fade` seconds when given. Negative volumes become 0, and a volume that is not finite raises `Audio needs a finite bus volume.`. The fade defaults to 0.

```lua
local audio = require('haylen.audio')

audio.setBusVolume('music', 0.5)
audio.setBusVolume('master', 0, 1.5)
```

### audio.busVolume(bus)

Returns the volume last set on a bus, which is the target of a fade in progress.

```lua
local audio = require('haylen.audio')

local percent = math.floor(audio.busVolume('sfx') * 100)
print(percent)
```

### audio.setBusMuted(bus, muted)

Mutes or unmutes a bus without changing its volume.

```lua
local audio = require('haylen.audio')

audio.setBusMuted('music', true)
```

### audio.busMuted(bus)

Returns whether a bus is muted.

```lua
local audio = require('haylen.audio')

audio.setBusMuted('sfx', not audio.busMuted('sfx'))
```

### audio.setBusProcessMode(bus, mode)

Sets the process mode that the voices of a bus inherit: `'inherit'`, `'pausable'`, `'whenPaused'`, `'always'` or `'disabled'`. Voices of child buses in `'inherit'` follow it too.

```lua
local audio = require('haylen.audio')
local haylen = require('haylen')

audio.setBusProcessMode('music', 'pausable')
haylen.setPaused(true)
```

### audio.busProcessMode(bus)

Returns the process mode of a bus as it was set, `'inherit'` included.

```lua
local audio = require('haylen.audio')

print(audio.busProcessMode('music'), audio.busProcessMode('sfx'), audio.busProcessMode('master'))
```

### audio.buses()

Returns a list of every bus name in alphabetical order.

```lua
local audio = require('haylen.audio')

for _, bus in ipairs(audio.buses()) do
    print(bus, audio.busVolume(bus), audio.busMuted(bus))
end
```

Bus volumes and mute states are saved and restored with `preferences.capture()` and `preferences.apply()` from [`haylen.preferences`](preferences.md).

### audio.busStats()

Returns one table per bus, in alphabetical order, with the voices that play through the bus itself, for debug statistics.

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | Name of the bus. |
| `voices` | integer | Voices on the bus. |
| `playing` | integer | Voices that are playing now. |
| `paused` | integer | Voices that the app, a pause of every voice, an interruption or the game pause holds. |
| `processing` | boolean | Whether the process mode of the bus runs in the current pause state. |

```lua
local audio = require('haylen.audio')

for _, bus in ipairs(audio.busStats()) do
    print(bus.name, bus.voices, bus.playing, bus.paused, bus.processing)
end
```

## Positional audio

A positional voice fades with its distance to the listener, pans by its horizontal offset and, with a Doppler factor, shifts its pitch while it and the listener approach or move apart. The engine refreshes every positional voice once per frame, after the scenes update, and measures velocities from the movement between frames. The [audio guide](../audio.md#positional-audio) explains the models.

### audio.setListener(x, y)

Moves the listener to a world position. The listener starts at `0, 0`. A coordinate that is not finite raises `Audio needs a finite position.`.

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

Returns the listener position as two numbers.

```lua
local audio = require('haylen.audio')

local x, y = audio.listener()
print(x, y)
```

### audio.followCamera(camera)

Moves the listener to the position of a [`Camera`](graphics2d.md#camera) plus its offset on every frame, without its shake, and keeps the camera alive while it is followed. The call `audio.followCamera(nil)` stops following and leaves the listener where it is. A value that is not a camera raises a bad argument error with `haylen.Camera expected`.

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

### audio.setSpatialization(options)

Changes how positional voices sound. It changes the fields it names and keeps the others, and unknown fields raise `Unknown option "<key>".`.

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `model` | string | `'linear'` | One of `'linear'`, `'inverse'` or `'exponential'`, the fade models of OpenAL. |
| `minDistance` | number | `100` | Within this distance a voice plays at full volume. |
| `maxDistance` | number | `1500` | The distance where fading stops. A linear fade reaches silence there with a rolloff of 1. |
| `rolloff` | number | `1` | How fast the volume falls between the two distances. |
| `panDistance` | number | `1500` | A voice this far to the side of the listener or farther plays only on that side. |
| `doppler` | number | `0` | The strength of the Doppler effect, where 0 turns it off. The pitch stays within two octaves up or down. |
| `speedOfSound` | number | `3430` | The speed of sound in world units per second. |

A field that is not finite raises `Audio needs a finite spatialization setting.`, distances outside `0 <= minDistance < maxDistance` raise `Audio attenuation needs 0 <= minimum distance < maximum distance.`, a minimum distance of 0 with the inverse or exponential model raises `The inverse and exponential audio models need a minimum distance above 0.`, a negative `rolloff` or `doppler` raises `The audio rolloff and Doppler factor cannot be negative.`, and a `panDistance` or `speedOfSound` that is not positive raises `The audio pan distance and speed of sound must be positive.`.

```lua
local audio = require('haylen.audio')

audio.setSpatialization({model = 'inverse', minDistance = 50, maxDistance = 1000, rolloff = 1.5})
audio.setSpatialization({doppler = 1, speedOfSound = 2000})
```

### audio.spatialization()

Returns every field of the spatialization in one table.

```lua
local audio = require('haylen.audio')

local settings = audio.spatialization()
print(settings.model, settings.minDistance, settings.maxDistance, settings.doppler)
```

## Output

### audio.sampleRate()

Returns the output sample rate in hertz, 48000 by default.

```lua
local audio = require('haylen.audio')

print(audio.sampleRate())
```

### audio.channels()

Returns the number of output channels, 2 by default.

```lua
local audio = require('haylen.audio')

print(audio.channels())
```

### audio.hasDevice()

Returns `true` when the mixer plays in real time for an audio device, which is the case in the player and in every app, even while the device is unavailable. The headless engine of tests and tools mixes without a device and returns `false`.

```lua
local audio = require('haylen.audio')

if not audio.hasDevice() then
    print('running headless, nothing is heard')
end
```

### audio.outputAvailable()

Returns `true` while the mix reaches an audio device. It returns `false` while the system refuses the device, such as on a computer without one, while another app holds the iOS audio session, or on a web page served over plain http, where browsers offer no `AudioWorklet`, and in the headless engine. The app runs all the same: voices, music, fades and positional audio go on in real time without sound, the log writes one warning with the reason when the output becomes unavailable, and the engine tries to open it again whenever the app becomes active or an interruption ends.

```lua
local audio = require('haylen.audio')

if audio.hasDevice() and not audio.outputAvailable() then
    print('the sound is off until this device offers an audio output')
end
```

The engine pauses the output while the app is suspended and resumes it afterwards, unless an interruption still holds it.

## Types

### Sound

The functions `assets.load()` and `assets.loadAsync()` return `haylen.Sound` userdata for sound files, and `audio.newSound()` for bytes in memory. Two handles of the same loaded sound compare equal with `==`. Every property is read-only.

| Property | Type | Meaning |
| --- | --- | --- |
| `duration` | number | Length in seconds. |
| `channels` | integer | Channels of the file, 1 for mono and 2 for stereo. |
| `sampleRate` | integer | Sample rate of the file in hertz. |
| `frameCount` | integer | Length in sample frames. |
| `streamed` | boolean | The value is `true` when the sound was loaded with the `stream` option. |

```lua
local assets = require('haylen.assets')

local hit = assets.load('sfx/hit.wav')
print(hit.duration, hit.channels, hit.sampleRate, hit.frameCount, hit.streamed)
```

### Filter

The type `haylen.Filter` is a second-order filter from `audio.newEffect` with a filter kind. Its parameters are read-write properties that tweens animate.

| Property | Type | Meaning |
| --- | --- | --- |
| `kind` | string | The kind it was created with. Read-only. |
| `cutoff` | number | Corner or center frequency in hertz. It must be positive, and cutoffs at or above half the sample rate act just below it. |
| `q` | number | Resonance of the pass and shelf kinds and width of the band of the others, where larger is narrower. It must be positive. |
| `gain` | number | Decibels that `'peak'`, `'lowShelf'` and `'highShelf'` add or remove, from -96 to 96. Setting it on another kind raises `Only peak and shelf filters have a gain.`. |
| `attached` | boolean | Whether it processes a bus or a voice now. Read-only. |
| `tail` | number | Seconds it keeps sounding after its input falls silent, 0 for filters. Read-only. |

```lua
local audio = require('haylen.audio')
local tween = require('haylen.tween')

local sweep = audio.newEffect('bandpass', {cutoff = 300, q = 4})
audio.addBusEffect('music', sweep)
tween.to(sweep, 2, {cutoff = 3000}, {repeatCount = -1, loopMode = 'yoyo'})
```

### Delay

The type `haylen.Delay` is an echo from `audio.newEffect('delay')`.

| Property | Type | Meaning |
| --- | --- | --- |
| `time` | number | Seconds between the input and each repeat, between 0 and `maxTime`. A change glides, bending the pitch of the repeats. |
| `maxTime` | number | The longest time, set when the delay is created. Read-only. |
| `feedback` | number | Level of each repeat relative to the one before, at least 0 and below 1. |
| `wet` | number | Level of the repeats, 0 or more. |
| `dry` | number | Level of the input, 0 or more. |
| `attached` | boolean | Whether it processes a bus or a voice now. Read-only. |
| `tail` | number | Seconds until the repeats fall 60 decibels below the first one. Read-only. |

A time outside its range raises `A delay time must be between 0 and the largest time of the delay.`, a feedback outside its range raises `A delay feedback must be at least 0 and below 1.`, a negative level raises `A delay wet level must be 0 or more.` or `A delay dry level must be 0 or more.`, and a `maxTime` that is not above 0 or is above 10 raises `A delay needs a largest time above 0 and up to 10 seconds.`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local echo = audio.newEffect('delay', {time = 0.3, maxTime = 1, feedback = 0.4, wet = 0.5})
audio.play(assets.load('sfx/shout.wav'), {effects = {echo}})
echo.time = 0.5
echo.feedback = 0.6
print(echo.tail)
```

### Reverb

The type `haylen.Reverb` is a Freeverb room from `audio.newEffect('reverb')`. Every parameter lies between 0 and 1, and values outside raise `A reverb <parameter> must be between 0 and 1.`.

| Property | Type | Meaning |
| --- | --- | --- |
| `roomSize` | number | Size of the room, which sets how long the reverb lasts. |
| `damping` | number | How much the reverb softens its high frequencies. |
| `width` | number | Stereo width of the reverb, from mono to fully spread. |
| `wet` | number | Level of the reverb. |
| `dry` | number | Level of the input. |
| `attached` | boolean | Whether it processes a bus or a voice now. Read-only. |
| `tail` | number | Seconds until the reverb falls 60 decibels. Read-only. |

```lua
local audio = require('haylen.audio')
local tween = require('haylen.tween')

local room = audio.newEffect('reverb', {roomSize = 0.3, wet = 0.2})
audio.addBusEffect('sfx', room)

-- Walking from a corridor into a hall.
tween.to(room, 1.5, {roomSize = 0.9, wet = 0.45, damping = 0.2})
```

## Events

The engine publishes these events on [`haylen.events`](events.md) for audio.

| Event | When |
| --- | --- |
| `audioInterrupted` | The system took the audio: a phone call, an alarm, Siri or another Android app with the audio focus. Every voice is paused and `audio.interrupted()` returns `true`. |
| `audioResumed` | The interruption ended while the app is active, or the app became active after it ended. The voices resume, and the audio session is active again unless the system still refuses the audio, which `audio.outputAvailable()` tells. |
| `audioRouteChanged` | The output moved to another device, such as headphones that were unplugged. |

```lua
local events = require('haylen.events')
local haylen = require('haylen')

events.on('audioInterrupted', function()
    haylen.setPaused(true)
end)

events.on('audioRouteChanged', function()
    print('the output device changed')
end)
```

## Errors

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | An options table has a key the call does not accept. |
| `Audio data is not a supported WAV, FLAC, MP3 or Ogg Vorbis file.` | The function `audio.newSound()` received bytes that are no sound file. |
| `Raw audio needs a sample rate and channels.` | The function `audio.newSound()` received raw samples without a `sampleRate` or `channels`. |
| `The audio bus "<name>" does not exist.` | A call names a bus that does not exist. |
| `An audio bus needs a non-empty name that no other bus uses, not "<name>".` | The function `audio.createBus()` received an empty or existing name. |
| `A pitch variation must be at least 0 and smaller than the pitch.` | The function `audio.play()` received a bad `pitchVariation`. |
| `Audio needs a finite volume.` | The function `audio.play()` or `audio.setVolume()` received a volume that is not finite, such as `0 / 0` or `math.huge`. |
| `Audio needs a finite pitch above 0.` | The function `audio.play()` or `audio.setPitch()` received a pitch of 0 or less or one that is not finite. |
| `Audio needs a finite pan.` | The function `audio.play()` or `audio.setPan()` received a pan that is not finite. |
| `Audio needs a finite fade-in.` | The function `audio.play()` received a `fadeIn` that is not finite. |
| `Audio needs a finite start time.` | The function `audio.play()` received a `startAt` that is not finite. |
| `Audio needs a finite position.` | The function `audio.play()`, `audio.setPosition()` or `audio.setListener()` received a coordinate that is not finite. |
| `Audio needs a finite bus volume.` | The function `audio.setBusVolume()` received a volume that is not finite. |
| `Audio needs a finite spatialization setting.` | The function `audio.setSpatialization()` received a field that is not finite. |
| `Audio attenuation needs 0 <= minimum distance < maximum distance.` | The function `audio.setSpatialization()` received bad distances. |
| `The inverse and exponential audio models need a minimum distance above 0.` | The function `audio.setSpatialization()` chose one of these models with a minimum distance of 0. |
| `The audio rolloff and Doppler factor cannot be negative.` | The function `audio.setSpatialization()` received a negative `rolloff` or `doppler`. |
| `The audio pan distance and speed of sound must be positive.` | The function `audio.setSpatialization()` received a `panDistance` or `speedOfSound` of 0 or less. |
| `The audio effect must be "lowpass", "highpass", "bandpass", "notch", "peak", "lowShelf", "highShelf", "delay" or "reverb", not "<kind>".` | The function `audio.newEffect()` received an unknown kind. |
| `The audio effect already processes another bus or voice.` | An effect was added while it is still on a bus or a voice, or still rings out after its voice ended. |
| `The option "effects" must be a list of audio effects.` | The function `audio.play()` received an `effects` value that is not a list of effects. |
| `A filter cutoff must be a positive frequency.` | A filter received a cutoff of 0 or less. |
| `A filter "q" must be positive.` | A filter received a `q` of 0 or less. |
| `Only peak and shelf filters have a gain.` | Another filter received a gain. |
| `A filter gain must be between -96 and 96 decibels.` | A peak or shelf filter received a gain outside that range or one that is not a number. |
| `audio effect expected` | A function that takes an effect received another value. It comes inside a bad argument error. |
| `expected a non-negative integer` | A voice id is negative. It comes inside a bad argument error. |
| `The type "haylen.Sound" has no member "<name>".` | A sound property does not exist. |
