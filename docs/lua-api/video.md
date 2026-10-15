# haylen.video

The module `haylen.video` plays video files of the package into a texture, which sprites, UI images, materials and anything else that draws a texture show, with its sound through the mixer and a bus of [`haylen.audio`](audio.md). Use it for intros, cutscenes, tutorials, animated backgrounds and menus.

```lua
local video = require('haylen.video')
```

Every platform decodes with its own decoder, so a video plays the formats the platform plays, and the engine ships no decoder of its own. The decoder opens on the I/O pool and decodes a few frames and a little sound ahead of the playback on the task pool, so neither opening nor decoding ever holds a frame up, and the texture changes once per frame at most. A video plays on real time, so the time scale never changes its speed, while its process mode decides whether the pause of the game holds it.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local video = require('haylen.video')
local viewport = require('haylen.viewport')

scene.push({
    enter = function(self)
        self.intro = video.load('videos/intro.mp4', {autoplay = true, owner = self})
        self.intro:on('ended', function() scene.replace(require('scenes.menu')) end, {owner = self})
    end,
    render = function(self)
        local area = viewport.visibleRect()
        graphics2d.beginScreen()
        graphics2d.draw(self.intro.texture, area.x, area.y, {width = area.width, height = area.height, pivotX = 0, pivotY = 0})
    end,
})
```

## Functions

### video.load(path, options)

Returns a `Video` of the file at `path`, relative to `content/`, which opens in the background and reports `ready` once it can play or `error` when it cannot. A file of the [content](content.md) that is not on the device yet comes down first. The video plays until its owner ends or `video:release()` releases it.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `autoplay` | boolean | `false` | Plays as soon as the video is ready. |
| `loop` | boolean | `false` | Plays again from the start at the end, without `ended`. |
| `rate` | number | `1` | The speed of the playback, from 0.25 to 4, whose sound plays faster and higher or slower and lower with it. |
| `volume` | number | `1` | The volume of the sound, zero or more. |
| `muted` | boolean | `false` | Silences the sound and keeps the volume. |
| `bus` | string | `'music'` | The bus of [`haylen.audio`](audio.md) the sound plays on. |
| `processMode` | string | `'inherit'` | Whether the pause of the game holds the video, its picture and its sound: `'inherit'` and `'pausable'` hold it, `'whenPaused'` plays only while the game is paused, `'always'` never holds it and `'disabled'` always does. |
| `owner` | table or userdata | the default owner | Releases the video when the owner ends, as the [owners of `haylen.events`](events.md#owners) describe. |

A path that does not exist raises `The video "<path>" does not exist.`, an unknown option raises `Unknown option "<key>".`, a rate out of range raises `The rate of a video must be from 0.25 to 4.` and a negative volume raises `The volume of a video must be zero or positive.`.

```lua
local video = require('haylen.video')

local background = video.load('videos/sea.mp4', {autoplay = true, loop = true, muted = true})
```

### video.loadAsync(path, options)

Returns a promise that resolves with the `Video` once it is ready, or rejects with the reason it failed, with the options of `video.load`.

```lua
local async = require('async')
local video = require('haylen.video')

async.spawn(function()
    local cutscene, failure = video.loadAsync('videos/ending.mp4'):await()
    if cutscene then
        cutscene:play()
    else
        print(failure)
    end
end)
```

## Video

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | read | The picture, transparent until the first frame shows, which keeps its handle when the video loops or seeks. |
| `width`, `height` | integer | read | The size of the picture in pixels, 0 until the video is ready. |
| `duration` | number | read | The length in seconds, 0 until the video is ready. |
| `time` | number | read and write | The time the playback is at, in seconds. It stops at the duration, where the video waits for its last frame before it ends, so it never depends on how fast the decoder runs. Setting it seeks. |
| `playing` | boolean | read | Whether the video plays. |
| `ended` | boolean | read | Whether the video played to its end. |
| `state` | string | read | `'loading'`, `'ready'` or `'failed'`. |
| `error` | string or `nil` | read | Why the video failed. |
| `frameCount` | integer | read | How many frames the texture showed. |
| `loop`, `rate`, `volume`, `muted` | | read and write | As the options of `video.load` describe. |

### video:play() and video:pause()

Play from the current time, or from the start once the video ended, and pause where it is. A video that is still loading plays once it is ready. A paused video shows the frame of its time.

### video:seek(seconds)

Moves the playback to the time, kept within the video, and shows the frame of that time as soon as it is decoded. A time that is not finite raises `A video seeks to a finite number of seconds.`.

### video:on(event, fn, options)

Listens to an event of the video and returns a `Connection`, with an `owner` in `options` that ends the listener, like the listeners of [`haylen.events`](events.md). An unknown event raises `Unknown video event "<event>". Videos report "ready", "ended" and "error".`.

| Event | When | Data |
| --- | --- | --- |
| `ready` | The video opened and shows its first frame. | None. |
| `ended` | The video played to its end without `loop`. | None. |
| `error` | The video cannot play. | `{message, code}`, where `code` is `'unsupported'` on a platform without a decoder of video files and `'failed'` for a file the platform cannot decode or that breaks while it plays. |

```lua
local video = require('haylen.video')

local trailer = video.load('videos/trailer.mp4')
trailer:on('ready', function() trailer:play() end)
trailer:on('error', function(error)
    print(error.code, error.message)
end)
```

### video:release()

Stops the video and its sound for good. The texture keeps the last frame it showed.

## Platforms

| Platform | Decoder |
| --- | --- |
| macOS, iOS, iPadOS, tvOS and Mac Catalyst | AVFoundation, through an asset reader that decodes the picture and the sound of the file, which it reads from the package by range, so a video in a zip or a protected release plays without a copy on disk. It plays what AVFoundation plays, such as H.264, HEVC and ProRes in MP4 and QuickTime files with AAC sound. |
| Android | MediaExtractor and MediaCodec of the NDK, through a media data source that reads the file from the package by range, with the decoders of the device, such as H.264, HEVC, VP8, VP9 and AV1 in MP4, WebM and Matroska files with AAC, Opus or Vorbis sound. The picture decodes into the surface of an image reader, whose images of YUV 4:2:0 describe their planes, so the engine turns them into RGBA whatever layout the decoder of the device writes. The media data source came with Android 9, so Android 8.1 reports `error` with the code `unsupported`. |
| Web | A video element of the page that nothing shows, made from the bytes of the file, which plays what the browser plays, such as H.264 in MP4 and VP9 and AV1 in WebM. The element keeps the clock, the engine copies its newest picture through a canvas, and its sound goes through an `AudioWorklet` into the mixer of the engine. A browser that refuses sound before the user interacted with the page plays the video muted until the first tap, click or key, and a page without sound plays it silent. The whole file reaches the element as a `Blob`, which the browser keeps outside the memory of the app. |
| Windows | The source reader of Media Foundation, through a COM stream that reads the file from the package by range, which plays what Windows plays, such as H.264 and HEVC in MP4 with AAC sound. |
| Linux | Not supported. No library that decodes video ships with every distribution: GStreamer and FFmpeg come in different versions and with different codecs, and the codecs of common formats such as H.264 often come apart from them, so `video.load` reports `error` with the code `unsupported`. |

## C++

The class `graphics::Video` plays a `platform::VideoDecoder`, the interface that the decoder of each platform implements and that a project implements for a format of its own: a decoder reads frames and sound ahead of the playback and seeks, and a decoder with a clock of its own plays by itself and reports its time. The `video` plugin creates videos of the package with `VideoPlugin::create`, which opens the file through `Host::openVideo`, and updates them at the start of every frame.

```cpp
std::shared_ptr<haylen::graphics::Video> video = engine.getPlugin<haylen::plugins::VideoPlugin>().create(engine, "videos/intro.mp4", {.autoplay = true});
video->ended.connect([] { showMenu(); });
```
