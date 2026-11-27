# 🎬 Video

This guide explains how an app plays video files with the module `haylen.video`: what a video is in Haylen, how a file goes from the package to the screen without holding up a frame, how to draw the picture and route the sound, how a video follows the pause of the game and its owner, which formats each platform plays and what to do where a platform plays none. The [`haylen.video` reference](../lua-api/video.md) lists every function, property, event and error.

## A video is a texture

A video in Haylen is not a window or a player of its own. It is a [`Video`](../lua-api/video.md#video) object that writes its current picture into a texture, and the app draws that texture like any other. So an intro covers the screen as a sprite, a tutorial clip shows inside a card of the UI, a menu has an animated background, and a monitor in a game level plays a clip on a shape with rounded corners. The same texture may draw several times in one frame.

The sound of the video plays through the mixer of [`haylen.audio`](../audio/audio.md), on a bus that the app picks, so the music volume of the settings also turns down the sound of a cutscene.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local video = require('haylen.video')

scene.push({
    enter = function(self)
        self.clip = video.load('videos/lighthouse.mp4', {autoplay = true, loop = true, owner = self})
    end,
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawShape({80, 60, 480, 270}, {radius = 16, texture = self.clip.texture})
    end,
})
```

## From the file to the screen

Video decoding is heavy work, so the engine keeps it off the frame thread. These steps run for each video.

1. [`video.load`](../lua-api/video.md#videoloadpath-options) returns at once with a video in the state `'loading'`. A file of the [content](../content/loading.md) that is not on the device yet downloads first.
2. The decoder of the platform opens the file on the I/O pool, the threads of the engine for slow input and output. The native decoders read the file from the package by range, so a video inside a zip or a protected release plays without a copy on disk, while a browser receives the whole file.
3. Once the decoder knows the size and the length, the video reports the event `ready`.
4. While the video plays, the task pool decodes a few frames and a little sound ahead of the current time.
5. On the frame thread, the video only picks the frame of its current time and hands it to the texture, at most once per frame. The decoded sound goes into the mixer.

The engine ships no decoder. Each platform decodes with its own media framework, which knows the video hardware of the device and adds nothing to the size of the app. The cost is that the formats differ from one platform to another, as [Formats and platforms](#formats-and-platforms) explains.

A video that loads while a scene shows needs no special care. To start a scene only once its clip is ready, load it with [`video.loadAsync`](../lua-api/video.md#videoloadasyncpath-options) in the `load` hook of the scene, which awaits the promise behind the loading view.

```lua
local scene = require('haylen.scene')
local video = require('haylen.video')

local Cutscene = {}

function Cutscene:load(context)
    local clip, failure = video.loadAsync('videos/ending.mp4', {owner = self}):await()
    self.clip = clip
    self.failure = failure
end

function Cutscene:enter()
    if self.clip then
        self.clip:play()
    end
end

scene.push(Cutscene)
```

## Drawing the picture

The [`texture`](../lua-api/video.md#properties) of a video stays the same object for the whole life of the video, through loops and seeks, so the app may keep it in a variable or hand it to the UI once. It is transparent until the first frame shows. The properties `width` and `height` give the size of the picture in pixels once the video is ready, which the app uses to keep the shape of the picture when it fits it into an area.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local video = require('haylen.video')
local viewport = require('haylen.viewport')

local function fit(clip, area)
    if clip.width == 0 then
        return nil
    end
    local scale = math.min(area.width / clip.width, area.height / clip.height)
    local width, height = clip.width * scale, clip.height * scale
    return area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height
end

scene.push({
    enter = function(self)
        self.intro = video.load('videos/intro.mp4', {autoplay = true, owner = self})
    end,
    render = function(self)
        graphics2d.beginScreen()
        local x, y, width, height = fit(self.intro, viewport.visibleRect())
        if x then
            graphics2d.draw(self.intro.texture, x, y, {width = width, height = height, pivotX = 0, pivotY = 0})
        end
    end,
})
```

To show a video inside the UI, give its texture a name with [`ui.setImage`](../lua-api/ui.md#uisetimagename-source) and use that name in an [`ui.image`](../lua-api/ui.md#uiimageproperties), the same way a minimap shows a render target. The property `aspectRatio` of a node keeps a frame of `16 / 9` whatever width the layout gives it. Remove the name with `ui.removeImage` when the screen leaves.

```lua
local ui = require('haylen.ui')
local video = require('haylen.video')

local clip = video.load('videos/how-to-jump.mp4', {autoplay = true, loop = true, muted = true})
ui.setImage('@tutorial', clip.texture)

ui.mount(ui.column{gap = 12,
    ui.label{text = 'Press the button to jump.'},
    ui.image{image = '@tutorial', aspectRatio = 16 / 9, radius = 12},
})
```

## Playing, pausing and seeking

The methods [`clip:play`](../lua-api/video.md#clipplay), [`clip:pause`](../lua-api/video.md#clippause) and [`clip:seek`](../lua-api/video.md#clipseekseconds) control the playback, and the properties `loop` and `rate` change it while it plays. A video that still loads starts once it is ready, so code can call `play` right after `load`. A seek shows the frame of the new time as soon as the decoder reaches it.

The video tells what happens through three events of [`clip:on`](../lua-api/video.md#cliponevent-fn-options): `ready`, `ended` and `error`. They reach Lua on the frame thread, so a listener may change scenes or the UI. A video that loops never reports `ended`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local video = require('haylen.video')

local Intro = {}

function Intro:finish()
    scene.replace(require('scenes.menu'))
end

function Intro:enter()
    self.clip = video.load('videos/intro.mp4', {autoplay = true, owner = self})
    self.clip:on('ended', function()
        self:finish()
    end, {owner = self})
end

function Intro:update(dt)
    if input.keyPressed('space') then
        self:finish()
    elseif input.keyPressed('right') then
        self.clip:seek(self.clip.time + 10)
    end
end

scene.push(Intro)
```

The `time` of a video follows the real clock, not the time scale of the app. A slow motion effect of the game therefore does not slow a cutscene down, while the `rate` of the video does, with its sound faster and higher or slower and lower. The time stops at the duration, where the video waits for its last frame before it ends. So `ended` comes after the last frame showed, even on a slow device.

## Sound

The sound of a video plays on the `music` bus unless the option `bus` names another one, such as a bus for cutscenes or for ambience. Its `volume` and `muted` change while it plays. A video used as a silent background should start with `muted` set, so it never plays a moment of sound. The [buses](../audio/audio.md#buses) section of the audio guide explains how buses mix and how the settings of a game control them.

On the web, a browser may refuse sound until the person interacted with the page. A video then plays muted until the first tap, click or key, and its picture is not held back.

## Pause, owners and the end of a video

A video follows the pause of the game through its process mode, like the voices of the audio module. With the default mode, the pause holds the picture and the sound together. A menu background that must keep moving while the game is paused takes the mode `'always'`. The [pause and process modes](../core/lifecycle.md#pause-and-process-modes) section explains every mode.

Every video has an owner, like timers and listeners. It plays until its owner ends, such as the scene that loaded it, or until [`clip:release`](../lua-api/video.md#cliprelease) stops it. A release ends the decoder and the sound at once and keeps the last picture in the texture, which suits a cutscene that ends on a still frame while the next scene fades in.

```lua
local scene = require('haylen.scene')
local video = require('haylen.video')

local Menu = {}

function Menu:enter()
    self.background = video.load('videos/menu-loop.mp4', {
        autoplay = true,
        loop = true,
        muted = true,
        processMode = 'always',
        owner = self,
    })
end

scene.push(Menu)
```

## Formats and platforms

A video plays the formats that its platform plays. The [platforms](../lua-api/video.md#platforms) section of the reference lists the decoder of each platform and what it reads. The format that every decoder of that table reads is H.264 in an MP4 file with AAC sound. Use it unless a platform needs something else, and keep the size of the picture to what the screen shows, since a smaller file decodes with less work and downloads faster.

Linux has no decoder that every distribution ships, and Android 8.1 lacks the media data source that the engine reads files through. There, and wherever a file does not decode, the video reports `error`. The code `unsupported` means the platform plays no video at all, and `failed` means this file did not play. An app that shows video on every platform plans for both: it skips an intro, or shows a still image of the clip instead.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local video = require('haylen.video')

local Trailer = {}

function Trailer:enter()
    self.clip = video.load('videos/trailer.mp4', {autoplay = true, loop = true, owner = self})
    self.clip:on('error', function(error)
        print('The trailer does not play: ' .. error.message)
        self.still = assets.texture('images/trailer-still.png')
    end, {owner = self})
end

function Trailer:render()
    graphics2d.beginScreen()
    local picture = self.still or self.clip.texture
    graphics2d.draw(picture, 0, 0, {width = 640, height = 360, pivotX = 0, pivotY = 0})
end

scene.push(Trailer)
```

## Performance

The frame thread does little for a video: it picks a frame and uploads it to the texture, once per frame at most, and the renderer keeps the memory of that upload so a video that changes every frame allocates nothing. The heavy work runs on the pools, and the media framework of the platform decodes on the video hardware of the device where it has one. So the cost that the app controls is the size of the picture, since each upload copies every pixel of it, and the number of videos that play at once. Pause or release a video that nobody sees, such as a monitor of a level that the camera left.

## From C++

A video in C++ is a `graphics::Video`, which the `video` plugin creates with `VideoPlugin::create` and updates at the start of every frame. It plays a `platform::VideoDecoder`, the interface that the decoder of each platform implements. A decoder either decodes ahead of the playback on the task pool, or keeps a clock of its own and plays by itself, as the video element of a browser does. A port of the engine brings the decoder of its platform through the service `port::VideoService`, as [ports](../platform/ports.md) describes, and the C++ section of the reference shows a video made from C++.
