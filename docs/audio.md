# Audio

Haylen mixes sound with [miniaudio](https://miniaud.io). Apps load sounds as assets, play them as voices, route voices through buses for volume sliders and effects, crossfade music tracks, place sounds in the world so they fade, pan and shift with distance and motion, and let the game pause stop the sounds it should stop. This guide explains those pieces, how the engine shares the audio output with the system and other apps, and how the Tiny Island sample uses them. The [haylen.audio reference](lua-api/audio.md) lists every function, option and error.

Every audio call runs on the frame thread and returns at once. The mixing itself runs on the audio thread of the platform, which Lua code never touches, and on the frame thread while the app has [no audio output](#without-an-audio-output). See [Architecture](architecture.md) for the threading model.

## Sounds

Sounds are assets of [haylen.assets](lua-api/assets.md) of the type `sound`, loaded from WAV, Ogg Vorbis, MP3 and FLAC files. There are two ways to hold a sound.

| Mode | How to load | Memory | Suits |
| --- | --- | --- | --- |
| Decoded | `assets.load('audio/ui/click.ogg')` | The whole sound as 32-bit float samples. | Short effects that play often, because a new voice reads samples that are already in memory. |
| Streamed | `assets.load('audio/music/day.mp3', 'sound', {stream = true})` | The encoded file. Each voice decodes it while it plays. | Music and long ambience, which would take a lot of memory decoded. |

A decoded sound is decoded when its asset loads, on worker threads when it loads through `assets.loadAsync` or a preload group. Preload group entries take the same options, so music can be preloaded as a stream:

```json
{
    "groups": {
        "gameplay": [
            "audio/effects/",
            "audio/ambient/fire_loop.ogg",
            {"path": "audio/music/day.mp3", "options": {"stream": true}}
        ]
    }
}
```

A loaded sound is a `haylen.Sound` value with the read-only properties `duration`, `channels`, `sampleRate`, `frameCount` and `streamed`. Two handles of the same cached sound compare equal with `==`.

## Voices

`audio.play(sound, options)` starts a voice and returns its id, an integer. The same sound can play on many voices at once, and each voice has its own volume, pitch, pan, position and playback cursor.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local crackle = audio.play(assets.load('audio/ambient/fire_loop.ogg'), {bus = 'ambience', loop = true, fadeIn = 1, volume = 0.7})
audio.stop(crackle, 0.5)
```

The options are `bus`, `volume`, `pitch`, `pitchVariation`, `pan`, `loop`, `fadeIn`, `startAt`, the world position `x` and `y`, the `processMode` that decides whether the voice plays while the game is paused, and the `effects` that process it. After it starts, a voice is controlled by id with `audio.stop(voice, fadeOut)`, `audio.pause`, `audio.resume`, `audio.setVolume`, `audio.setPitch`, `audio.setPan`, `audio.setPosition`, `audio.addEffect` and `audio.removeEffect`, and read with `audio.active`, `audio.paused`, `audio.cursor`, `audio.pitch`, `audio.processMode` and `audio.effects`.

- A voice id stays safe after the voice ends. Calls with the id of a finished voice do nothing, and `audio.active` returns `false`, so an app never has to check a voice before stopping it.
- A paused voice keeps its cursor and still counts as active, while a stopped voice stops counting as active at once, even while it fades out.
- Volumes, pitches, pans and positions must be finite numbers, and pitches above 0, so a stray `0 / 0` raises an error instead of silencing a bus and its effects for good.
- The engine releases finished voices once per frame, and `audio.voiceCount()` counts a voice that just ended until then.
- At most 128 voices play at once. When the limit is reached, a new voice stops the oldest voice that is not music.
- `audio.stopAll(fadeOut)` stops every voice, music included.

### Pitch variation

Sounds that repeat, such as footsteps, hits and clicks, sound mechanical when every play is identical. `pitchVariation` picks the pitch of each play at random between `pitch - pitchVariation` and `pitch + pitchVariation`. It must be at least 0 and smaller than `pitch`. `audio.pitch(voice)` returns the pitch that was picked, and `audio.seedVariation(seed)` reseeds the generator behind the picks, which makes them repeatable for tests and replays.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

audio.play(assets.load('audio/effects/footstep_1.ogg'), {volume = 0.35, pitchVariation = 0.08})
```

## Buses

Every voice plays through a bus, and buses form a tree whose root, `master`, feeds the output. The buses `music`, `sfx`, `ui` and `ambience` exist from the start as children of `master`. Voices use `sfx` and music uses `music` unless the options name another bus.

```text
master
├── ambience
├── music
├── sfx
└── ui
```

The volume of a voice is multiplied by the volume of its bus and of every bus above it, so a settings screen changes whole groups of sounds with one call. A bus also has a [process mode](#pause-and-process-modes) that its voices inherit and a chain of [effects](#effects) that processes everything it mixes.

| Function | Use |
| --- | --- |
| `audio.createBus(name, parent)` | Adds a custom bus under `parent`, which defaults to `master`, such as `footsteps` under `sfx` or `voices` under `master`. |
| `audio.setBusVolume(bus, volume, fade)` | Changes a bus volume, fading over `fade` seconds when given. |
| `audio.busVolume(bus)` | Returns the volume last set, which is the target of a fade in progress. |
| `audio.setBusMuted(bus, muted)` and `audio.busMuted(bus)` | Mute a bus without changing its volume, so unmuting restores the level the player chose. |
| `audio.buses()` | Lists every bus in alphabetical order. |

```lua
local audio = require('haylen.audio')

audio.createBus('footsteps', 'sfx')
audio.setBusVolume('music', 0.5)
audio.setBusVolume('master', 0, 1.5)
```

[haylen.preferences](lua-api/preferences.md) saves bus volumes and mute states with `preferences.capture()` under the keys `audio.volume.<bus>` and `audio.muted.<bus>`, and restores them with `preferences.apply()`. Create custom buses before calling `preferences.apply()`, because buses that do not exist yet are skipped.

## Music

Music is one track at a time on its own voice. `audio.playMusic(sound, options)` starts a track and crossfades from the previous one over `fade` seconds, 1 by default: the old track fades out while the new one fades in. Music loops unless `loop = false`, and plays on the `music` bus unless `bus` names another.

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

- `audio.playMusic` returns the voice id of the track, which every voice function takes: `audio.pause` and `audio.resume` pause the music alone, and `audio.setVolume`, `audio.addEffect` and `audio.stop` reach the track.
- Asking for the track that is already playing keeps it going on the same voice, paused or not, and only changes its volume, so a scene can call `playMusic` in `enter` without restarting the music every time the player returns to it.
- A paused track that the next one replaces stops without sounding again, and the next track plays. A track that cannot play raises an error and leaves the current track playing.
- `audio.stopMusic(fadeOut)` fades the track out over 1 second by default, and `audio.stopMusic(0)` stops it at once.
- `audio.music()` returns the sound that is playing or paused, or `nil`, and a track stopped through its voice counts as over.
- The music voice is the last one the voice limit stops.

## Pause and process modes

The game pause of `haylen.setPaused` reaches sounds with the same process modes that scenes, timers and tweens use, as the [lifecycle guide](lifecycle.md#pause-and-process-modes) explains. A voice plays only while its process mode runs in the current pause state. A voice that the pause stops keeps its cursor, still counts as active, and resumes where it stopped when the pause ends.

Every bus has a process mode that its voices inherit. A bus in `'inherit'` takes the mode of its parent, and `master` resolves `'inherit'` to `'pausable'`. The buses start with the modes most games want: effects and ambience stop with the game, while music and interface sounds keep playing, so a pause menu keeps its music and its clicks.

| Bus | Mode |
| --- | --- |
| `master` | `'inherit'`, which counts as `'pausable'` |
| `music` | `'always'` |
| `ui` | `'always'` |
| `sfx` | `'pausable'` |
| `ambience` | `'pausable'` |
| Custom buses | `'inherit'`, the mode of their parent |

`audio.setBusProcessMode(bus, mode)` changes the mode of a bus, and the `processMode` option of `audio.play` gives one voice a mode of its own. A countdown that must keep ticking through the pause plays as `'always'`, the music of a pause menu plays as `'whenPaused'`, and a game whose music should stop with it moves the `music` bus to `'pausable'`.

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

Voices also pause for reasons of their own, and each reason holds a voice separately, so ending one pause never resumes a voice that another reason still holds.

- `audio.pause(voice)` is the pause of one voice by the app, and `audio.paused(voice)` reads it.
- `audio.pauseAll()` pauses every voice that exists at that moment, such as while a cutscene or a dialog takes over, and `audio.resumeAll()` resumes them. A voice that the app paused by itself stays paused, and voices started in between play as usual.
- A system [interruption](#sessions-and-interruptions) pauses every voice for a reason of its own in the same way.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local radio = audio.play(assets.load('sfx/radio.ogg'), {bus = 'ambience', loop = true})
local rain = audio.play(assets.load('sfx/rain.ogg'), {bus = 'ambience', loop = true})
audio.pause(radio)

audio.pauseAll()
audio.play(assets.load('sfx/dialog_open.wav'), {bus = 'ui'})
audio.resumeAll()

-- The rain plays again, and the radio that the player turned off stays off.
print(audio.paused(radio), audio.paused(rain))
```

`audio.busStats()` reports, for every bus, how many voices play through it, how many of them are playing and paused, and whether its process mode runs in the current pause state, which suits debug overlays and tests.

```lua
local audio = require('haylen.audio')

for _, bus in ipairs(audio.busStats()) do
    print(bus.name, bus.voices, bus.playing, bus.paused, bus.processing)
end
```

## Effects

Effects process sound inside the mixer, as nodes of the miniaudio node graph. An effect on a bus processes everything the bus mixes, and an effect on a voice processes that voice before its bus. `audio.newEffect(kind, options)` creates one, and every option is optional.

| Kind | Parameters | Use |
| --- | --- | --- |
| `'lowpass'` | `cutoff`, `q` | Removes the highs above the cutoff: a sound behind a wall, underwater, the game under a menu. |
| `'highpass'` | `cutoff`, `q` | Removes the lows below the cutoff: a radio, a phone, a thin intercom. |
| `'bandpass'` | `cutoff`, `q` | Keeps a band around the cutoff, narrower as `q` grows. |
| `'notch'` | `cutoff`, `q` | Removes a band around the cutoff, such as a hum. |
| `'peak'` | `cutoff`, `q`, `gain` | Lifts or cuts a band around the cutoff by `gain` decibels, the band of an equalizer. |
| `'lowShelf'`, `'highShelf'` | `cutoff`, `q`, `gain` | Lifts or cuts everything below or above the cutoff by `gain` decibels, the bass and treble of an equalizer. |
| `'delay'` | `time`, `maxTime`, `feedback`, `wet`, `dry` | Repeats the sound after `time` seconds, each repeat at `feedback` times the one before: canyon echoes or a slapback. |
| `'reverb'` | `roomSize`, `damping`, `width`, `wet`, `dry` | Places the sound in a space, from a small room at `roomSize = 0` to a large hall at `1`. |

The filters are the second-order filters of the Audio EQ Cookbook of Robert Bristow-Johnson. `cutoff` is in hertz, 1000 by default, and `q` defaults to 0.7071, the flattest response without a bump. The delay keeps `maxTime` seconds of sound, 1 by default and at most 10, which is the longest `time` it accepts. The reverb is Freeverb, the Schroeder and Moorer design of parallel comb filters and series allpass filters that most game reverbs grew from, with every parameter between 0 and 1.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

-- A cave: every sound effect rings.
audio.addBusEffect('sfx', audio.newEffect('reverb', {roomSize = 0.85, damping = 0.3, wet = 0.4}))

-- One shout with an echo of its own.
local echo = audio.newEffect('delay', {time = 0.35, feedback = 0.45, wet = 0.6})
audio.play(assets.load('sfx/shout.wav'), {effects = {echo}})
```

- Effects on one bus or voice run in the order they were added, and `audio.busEffects(bus)` and `audio.effects(voice)` list them in that order.
- Parameters are properties that change at any time, such as `muffle.cutoff = 400`. The audio thread applies them from the next block it mixes, and tweens animate them natively, without running Lua on every frame.
- An effect processes one bus or one voice at a time, and adding it to a second one raises an error until it leaves the first. Effects that many sounds share belong on a bus, and each voice that needs its own effect gets its own.
- A voice lets its echo and its reverb ring out after it ends, for as long as they take to fall silent, and the effect is free for another voice afterwards. An effect that moves to another bus or voice starts there without the echoes or reverb it held before.

```lua
local audio = require('haylen.audio')
local events = require('haylen.events')
local tween = require('haylen.tween')

-- The music, which keeps playing through the pause, grows muffled while the game is paused.
local muffle = audio.newEffect('lowpass', {cutoff = 20000})
audio.addBusEffect('music', muffle)

events.on('paused', function()
    tween.to(muffle, 0.3, {cutoff = 500}, {processMode = 'always'})
end)
events.on('unpaused', function()
    tween.to(muffle, 0.3, {cutoff = 20000}, {processMode = 'always'})
end)
```

Haylen writes its effects as its own miniaudio nodes instead of using the filter and delay nodes of miniaudio. The nodes of miniaudio change their parameters by rewriting their coefficients from the calling thread while the audio thread reads them, and their delay cannot change its time. The effects of Haylen keep their parameters in atomic values that the audio thread picks up on its next block, and the delay glides to a new time, which bends the pitch of its repeats like a tape delay instead of clicking.

## Positional audio

A voice becomes positional when `audio.play` receives `x` or `y`, or when `audio.setPosition` moves it. The listener is a point in the world, at `0, 0` until `audio.setListener(x, y)` moves it or `audio.followCamera(camera)` ties it to a camera. The engine refreshes every positional voice once per frame, after the scenes update, so moving the listener or the voices in `update` is enough.

`audio.setSpatialization(options)` decides how a positional voice sounds relative to the listener. It changes the fields it names and keeps the others.

| Field | Default | Meaning |
| --- | --- | --- |
| `model` | `'linear'` | How the volume falls with distance: `'linear'`, `'inverse'` or `'exponential'`. |
| `minDistance` | `100` | Within this distance a voice plays at full volume. |
| `maxDistance` | `1500` | The distance where fading stops. |
| `rolloff` | `1` | How fast the volume falls between the two distances. |
| `panDistance` | `1500` | A voice this far to the side of the listener or farther plays only on that side. |
| `doppler` | `0` | How strongly motion shifts the pitch, where 0 turns the Doppler effect off. |
| `speedOfSound` | `3430` | The speed of sound in world units per second, 343 meters per second at 10 units per meter. |

The fade models are those of OpenAL, measured with the distance clamped between the minimum and the maximum distance.

| Model | Volume at distance `d` |
| --- | --- |
| `'linear'` | `1 - rolloff * (d - minDistance) / (maxDistance - minDistance)`, which reaches silence at the maximum distance with a rolloff of 1. |
| `'inverse'` | `minDistance / (minDistance + rolloff * (d - minDistance))`, the natural fade of sound, which never reaches silence. |
| `'exponential'` | `(d / minDistance) ^ -rolloff`. |

`'inverse'` and `'exponential'` need a minimum distance above 0, and they keep the volume they reach at the maximum distance for voices farther away. The linear model suits most 2D games, because a sound off the screen should fall silent.

A voice pans by its horizontal offset from the listener over the pan distance, added to its own `pan`, so a voice to the right sounds on the right and a voice above or below the listener stays centered. With a Doppler factor above 0, the pitch of a voice rises while it and the listener approach each other and falls while they move apart, by the formula of OpenAL and within two octaves up or down. The engine measures the velocities of the voices and of the listener from their movement between frames, so an app only moves them. `audio.pitch(voice)` keeps returning the pitch the app set, without the shift.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')

local chop = assets.load('audio/effects/chop.ogg')
local camera = graphics2d.newCamera()
audio.setSpatialization({maxDistance = 1200, panDistance = 800, doppler = 1})
audio.followCamera(camera)

function game:update(dt)
    camera:follow(self.player.x, self.player.y, dt)
end

function game:chop(tree)
    audio.play(chop, {x = tree.x, y = tree.y, pitchVariation = 0.08})
end
```

A followed camera moves the listener to its position plus its offset, without its shake, and the engine keeps the camera alive while the listener follows it. `audio.followCamera(nil)` stops following and leaves the listener where it was. Only volume, pan and pitch change with position, so a positional sound stays a plain stereo sound, and a lowpass effect on the voice adds the muffling of distance or walls when a game wants it.

## Sessions and interruptions

The output runs at 48000 Hz in stereo by default, as `audio.sampleRate()` and `audio.channels()` report. The engine owns the audio context and the playback device, so it decides how the app shares sound with the system and with other apps.

On iOS and tvOS, the `audio` object of `app.json` chooses the category of the audio session.

| `iosSession` | Silent switch | Other apps | Suits |
| --- | --- | --- | --- |
| `'ambient'` | Silences the app. | Keep playing, mixed with the app. | Games, the default, so a player can listen to their own music. |
| `'soloAmbient'` | Silences the app. | Stop. | Games whose sound is part of the play. |
| `'playback'` | Keeps playing. | Stop, unless `mixWithOthers` is `true`. | Music players, radios, meditation and other apps that play audio as their purpose. |

```json
{
    "name": "Night Radio",
    "identifier": "dev.haylen.nightradio",
    "audio": {"iosSession": "playback", "mixWithOthers": false}
}
```

`mixWithOthers` defaults to `false`, and only the playback category accepts `true`, because ambient always mixes and solo ambient never does. Unknown categories and `mixWithOthers` with another category are errors of `app.json`. On Android the device plays through AAudio with the usage `game` and the content type `sonification`, which is how the system routes, ducks and mixes a game, and the Android activity requests the audio focus with the same attributes.

The system can take the audio away for a phone call, an alarm, Siri or another app. An interruption stops the output, pauses every voice for its own reason and publishes `audioInterrupted`, and `audio.interrupted()` returns `true` until it ends. When it ends, the engine opens the audio device again, which reactivates the iOS audio session with its category, restarts the output unless the app is in the background, resumes the voices and publishes `audioResumed`. An interruption that ends while the app is not active waits until the app becomes active again, because iOS gives the audio back only to an active app. When the system still refuses the audio at that moment, the interruption ends all the same and the app goes on [without an audio output](#without-an-audio-output) until the system hands it back. When the output moves to another device, such as headphones that are unplugged, the engine publishes `audioRouteChanged`, and a music player pauses its track there.

```lua
local audio = require('haylen.audio')
local events = require('haylen.events')
local haylen = require('haylen')

events.on('audioInterrupted', function()
    -- A call came in: pause the run too, so the player comes back to the pause menu.
    haylen.setPaused(true)
end)

events.on('audioResumed', function()
    print('the audio is back', audio.interrupted())
end)

events.on('audioRouteChanged', function()
    print('the headphones changed')
end)
```

Interruptions reach the engine in two ways, and both take the same path. The miniaudio device reports the interruptions and route changes of the iOS audio session and the route changes of AAudio. The platform reports what the device cannot see as the platform events `InterruptionBegan` and `InterruptionEnded`, which also make the app inactive while they last. On Android the activity maps the audio focus onto them: a loss of focus, lasting or transient, begins an interruption, and a gain of focus ends it, including the focus the activity requests again when it returns to the foreground, while a transient loss that lets the app duck does not interrupt the audio, since the system lowers its volume. When the device and the platform report the same interruption, the engine pauses and resumes once and publishes each event once. C++ hosts report device events of their own with `Mixer::reportDeviceEvent` from any thread.

When the app goes to the background, the engine stops the audio output, and it starts the output again when the app comes back, unless an interruption still holds it. On the web, a page that pauses the app with `Module.haylen.pause()` suspends it the same way.

On the web the engine plays through an audio output of its own, which is the only backend miniaudio has there. The mixer runs on the page thread like the rest of the app and mixes blocks of 256 frames, which the page posts over the message port of an `AudioWorkletNode`. Its processor, `haylen-audio-worklet.js` next to the runtime script, keeps them in a small ring buffer and plays them on the audio thread of the browser, and answers with the frames it played, so the page mixes new blocks until 50 ms of audio wait ahead of the output again. A frame of the app that takes much longer than usual therefore does not starve the output. The output needs no `SharedArrayBuffer`, no COOP or COEP headers and no WebAssembly threads. Browsers offer `AudioWorklet` only to pages served over https or from localhost, so on any other page, such as one served over plain http from the LAN address of a computer, the app runs [without an audio output](#without-an-audio-output) and the console says why. The `audio` entry of [`onStats`](build.md#runtime-api) reports whether the output is available, its state, the audio waiting in it and the render quanta it played without samples.

Browsers keep audio silent until the player interacts with the page. The runtime resumes the output at every tap, click or key while the app plays in the foreground, which also brings it back after Safari on iOS suspends it for an interruption. Until the unlock the output fills its 50 ms and then asks for no more blocks, so voices started earlier, such as the menu music, begin when the player first clicks or taps. The engine adds no unlock screen of its own, so a web app whose first screen matters for sound should wait for a click, as a title screen with a start button does.

## Without an audio output

An app never stops because it has no audio output. A computer without an audio device, an iOS audio session that another app holds, or a web page served over plain http, where the browser offers no `AudioWorklet`, leaves the mixer without an output. The app runs as usual, the log writes one warning with the reason, and `audio.outputAvailable()` returns `false`, so an app can tell the player that its sound is off.

Without an output the mixer keeps time itself. Every frame it mixes the seconds that passed and throws the samples away, so voices play to their end, loops repeat, fades and music crossfades finish, effect tails ring out, the voice limit stops the oldest voice and positional voices follow the listener, exactly as they would out loud. Time stands still for the audio while the app is in the background or an interruption holds it, as it does with an output.

An interruption that ends while the system still refuses the audio ends all the same: the voices resume, the engine publishes `audioResumed`, the log warns once and the app goes on without an output. Whenever the app becomes active and whenever an interruption ends, the engine tries again to open the output, without warning again, so the sound comes back as soon as the system offers it.

```lua
local audio = require('haylen.audio')
local events = require('haylen.events')

local function showSoundHint()
    if not audio.outputAvailable() then
        print('this device plays no sound right now')
    end
end

showSoundHint()
events.on('appActive', showSoundHint)
```

## The Tiny Island sound module

`samples/games/tiny-island/source/systems/sound.lua` wraps `haylen.audio` in four functions that the rest of the game calls by name. It is a compact model for an app's sound module.

A table describes every effect by name with its bus, its recordings and an optional volume. Effects with several recordings, such as `chop` or `footstep`, pick one at random on each play.

```lua
local effects = {
    click = {bus = 'ui', files = {'ui/click.ogg'}},
    chop = {bus = 'sfx', files = {'effects/chop.ogg', 'effects/wood_hit_1.ogg', 'effects/wood_hit_2.ogg', 'effects/wood_hit_3.ogg'}},
    footstep = {bus = 'sfx', files = {'effects/footstep_1.ogg', 'effects/footstep_2.ogg', 'effects/footstep_3.ogg', 'effects/footstep_4.ogg'}, volume = 0.35},
    dawn = {bus = 'ui', files = {'jingles/dawn.ogg'}},
}
```

`sound.play(name, x, y)` picks a recording, adds a pitch variation of 0.08 and makes the voice positional when the caller passes a world position. Gameplay sounds such as chopping, hits and footsteps pass their position, so they pan and fade with the distance to the player, while interface sounds pass none.

```lua
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
```

`sound.loop(file, volume)` starts a looping voice on the `ambience` bus with a one-second fade in, which the campfire uses for its crackle, and `sound.stop(voice)` fades a voice out over half a second when the campfire is destroyed.

`sound.music(name, fade)` loads a track as a stream and crossfades to it with `audio.playMusic`. The menu plays `menu`, a run starts with `day`, dusk crossfades to the `night` ambience over 3 seconds and dawn crossfades back, and the game over track plays once without looping.

```lua
function sound.music(name, fade)
    local track = assets.load('audio/' .. music[name], 'sound', {stream = true})
    audio.playMusic(track, {fade = fade or 1.5, loop = name ~= 'gameOver'})
end
```

The rest of the game completes the picture.

- `systems/game.lua` moves the listener to the player every frame with `audio.setListener`, keeping the default attenuation of 100 and 1500 world units.
- `systems/preferences.lua` applies the two volume sliders of the settings screen to the buses: the music slider sets the `music` bus, and the effects slider sets `sfx`, `ui` and `ambience`. Sliders call it every frame they move, and the values reach storage when the settings screen closes.
- `content/preload.json` preloads the interface sounds in the `boot` group, the menu music as a stream in the `menu` group, and the effects, jingles, ambience and the other tracks in the `gameplay` group, so no sound loads in the middle of a run.

## From C++

`haylen::audio::Mixer` (`haylen/audio/Mixer.hpp`), reached with `engine.getAudio()`, offers the same voices, buses, music, process modes, effects and positional audio with its `Mixer::PlayOptions`, `Mixer::MusicOptions` and `Mixer::Spatialization` structures, and `haylen::audio::Sound` (`haylen/audio/Sound.hpp`) holds decoded or streamed sound data. The effects are `audio::Filter`, `audio::Delay` and `audio::Reverb`, created with `std::make_shared` and their settings structures, and `audio::Session` is the session of `app.json`. The engine applies its pause through `Mixer::setProcessPaused`, and `Mixer::deviceEventReceived` delivers device events on the frame thread, which the audio plugin turns into the interruption behavior and the events above. `Mixer::isOutputAvailable` tells whether the mix reaches an audio device. A mixer created without a device mixes on demand through `render`, which tests and offline tools use.

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
