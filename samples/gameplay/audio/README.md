# Haylen Audio

A Lua sample with one scene per feature of [haylen.audio](../../../docs/lua-api/audio.md) and the [audio guide](../../../docs/audio.md). The menu lists the tests, each test opens as its own scene with a Back button, its live values and the calls it makes, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu. Every test preloads the sounds of the sample in its `load` hook while the fade covers the screen: the effects decode into memory and the music streams from its file.

| Test | What it shows |
| --- | --- |
| Music | Two streamed tracks that crossfade over the chosen time, looping or played once, a volume that `playMusic` changes on the track already playing, the pause and resume of the music through the voice `playMusic` returns, and a graph of the fades the calls asked for. |
| One-shot effects | A sound played with a volume, a pitch with a random variation, a pan, a fade in and a fade out, a limit of voices per sound that stops the oldest one, and every voice with the pitch it got, its pan and its cursor. |
| Buses | Music, effects, footsteps on a custom bus, interface clicks and ambience through the bus tree, with the volume, mute and process mode of every bus, the game pause and `audio.busStats()`. |
| Effects | Every filter kind, the delay and the reverb on the music bus or on a looping voice, with a slider per parameter, a tween that sweeps the main parameter, and the response of the filter, the repeats of the delay or the decay of the reverb. |
| Positional audio | Campfires, a woodcutter and a circling hum in a world around a listener that follows the camera, with the fade models, the distances, the rolloff, the pan distance, the Doppler effect and the volume, pan and pitch each sound gets. |
| Interruptions and lifecycle | The `audioInterrupted`, `audioResumed` and `audioRouteChanged` events and the app states as they happen while a track plays, the state of the output, which is unavailable where the app runs without sound, and the lifecycle options that mute or halt the app when it is not active. |
| Many voices | Bursts and a rain of voices up to the limit of 128, the count of voices over time and the statistics of every bus, while the music plays on. |

The recordings come from the Tiny Island sample and are CC0, credited in `content/CREDITS.md`. The hum and the plucked loop were synthesized for this sample.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run gameplay/audio` |
| macOS app | `python3 make.py run gameplay/audio --platform macos` |
| iPhone and iPad simulator | `python3 make.py run gameplay/audio --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run gameplay/audio --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run gameplay/audio --platform android --device <serial>` |
| Browser | `python3 make.py run gameplay/audio --platform web` |

Browsers keep the sound silent until the first click, tap or key on the page, so the web build starts playing once the player picks a test.

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Use the panel of a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Play a sound at a pan in the one-shot test | Click the stage | Play button | Tap the stage | Play button |
| Walk the listener in the positional test | WASD, or click where to go | Right stick | Touch stick | |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
