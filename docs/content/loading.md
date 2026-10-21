# 📥 Loading and delivering content

This guide explains, from the side of an app, how its Lua code and its assets reach the device and then memory. It covers what happens to `source/` and `content/` in development and in a release, when autoloads run, how assets load at once, in the background and in preload groups, and how scenes load them. It also covers when the bytes of an asset are on the device, which caches hold what, how each platform and store ships the same content, and how to publish updates and keep your keys. The [content guide](content.md) describes the formats, the cryptography, the tools and the local store in depth.

## Development and release

An app runs in development whenever it plays its package as it is. That happens in the desktop player of `haylen run <app>`, and in the Debug configuration of `haylen run` and `haylen prepare` on every platform. It runs as a release in the Release configuration, which ships the package only as its protected release, as the [distribution guide](../tools/distribution.md#release-builds) describes.

| Folder | Development | Release |
| --- | --- | --- |
| `source/` | The Lua files ship as text. The engine compiles a module the first time `require` asks for it. With `--dev` or the development server, a saved module reloads in place, as [hot reload](../core/lua.md#hot-reload) describes. | The content tool compiles every module into Lua bytecode when the release builds. The bytecode goes with `app.json` and the manifest and Lua modules of every plugin into the app domain, `app.hmanifest` and its shards, encrypted and signed. No Lua text ships. Calls such as `require('scenes.menu')`, the autoloads and `source/main.lua` resolve the same names as in development. The app domain always ships with the build of the app and never downloads, as [Lua bytecode](content.md#lua-bytecode) describes. |
| `content/` | The files ship as they are, in a folder, a zip archive or the bundle, and `assets` reads them directly. The delivery rules of `app.json` still apply, as [development](#development) describes. A saved file reloads its assets in place. | Every file is split into chunks, which are compressed, encrypted and authenticated one by one and stored in the shards of the content domain, `content.hmanifest`. A file keeps its bytes, so it decodes exactly as in development. The shards ship with the app, with two exceptions that [delivery](#delivery) describes: the shards that only later deliveries use stay on the server when the app names one, and the shards of packs go to the store that delivers them. |

The only file of the package that a release shows outside its shards is the splash logo, which the system draws before the engine starts. Nothing in app code changes between the two. The same `assets.load`, `assets.loadAsync`, preload groups and scene hooks work on a folder, a zip archive and a protected release.

## A small game needs no configuration

An app without a `content` section in `app.json` has every asset `required`. Everything ships with the app, and nothing downloads. In the Release configuration the engine chunks, compresses, encrypts, signs and lays out the release on its own. The folder of the app is the ordinary package.

```text
my-game/
  app.json
  source/
    main.lua
    scenes/
  content/
    images/
    audio/
    maps/
    preload.json
```

```json
{
    "name": "My Game",
    "identifier": "com.example.mygame",
    "version": "1.0.0",
    "design": {"width": 1920, "height": 1080, "scaling": "expand"}
}
```

These commands build what each store receives. The first release build of an app creates its keys and asks you to back them up, as [keys](#keys) describes.

```sh
# Apple: writes the release and the bootstrap into the Apple project, which Xcode archives for the App Store.
haylen prepare ~/apps/my-game --platform ios --config Release
# Apple: builds, inspects and runs the release on a device or a simulator.
haylen run ~/apps/my-game --platform ios-simulator --config Release

# Android: creates the upload key once, then builds, inspects and installs the release APK.
haylen android-key ~/apps/my-game
haylen run ~/apps/my-game --platform android --config Release
# Android: writes the release and the library of the app into the Android project, whose Gradle builds the App Bundle for Google Play.
haylen prepare ~/apps/my-game --platform android --config Release

# Steam: builds the release on a machine of the platform and stages its depots for SteamPipe.
haylen steam ~/apps/my-game --platform windows --app-id 1000 --depot windows=1001

# Web: makes the site in build/apps/my-game-<hash>/web/, or serves it on this machine with byte ranges.
haylen prepare ~/apps/my-game --platform web --config Release
haylen run ~/apps/my-game --platform web --config Release
```

For each of them the engine does the same steps:

1. It compiles the shaders whose sources changed and compiles the Lua modules into bytecode.
2. It builds the two domains of the release with the profile of the platform, `apple`, `android`, `windows`, `linux` or `web`, reusing the shards of the previous release and the build cache of the app.
3. It writes the bootstrap that seals the keys of the app into its binary.
4. It places the release where the runtime of the platform reads it.

The commands `run` and `steam` also inspect the built app for anything that a release never holds. [Platforms and stores](#platforms-and-stores) shows what each of them contains.

## Autoload, preload, delivery and cache

Four mechanisms decide when code and assets are ready. Each solves a different problem.

| Concept | What it decides | Lifetime | Downloads |
| --- | --- | --- | --- |
| [Autoload](#autoloads) | Lua modules of the app that load before `source/main.lua` and serve every scene. | The whole run of the app. | Never, since the code of an app ships with its build. |
| [Preload group](#preload-groups) | Assets that load and decode into memory together and stay there. | Until `assets.unloadGroup`. | It downloads its assets that are not on the device before it loads them. |
| [Delivery](#delivery) | When the encrypted bytes of an asset are on the device: before the start, in the background or on request. | On the disk, in the app or in the local store. | Assets with `prefetch` and `onDemand` download, and required assets only when the device lacks them, as on the web. |
| [Asset cache](#caches-and-memory) | The reuse of an asset that is already decoded in memory. | While anything holds the asset. | Never by itself. |

A delivery never decodes an asset into memory, and a preload group never decides where bytes are stored. So a game of several gigabytes with every asset required starts with only the assets that its first scenes load in memory.

## Startup

The engine starts an app in a fixed order.

1. The runtime opens the package. A release verifies the signatures of both manifests, checks them against the build of the app and decrypts their catalogs, which name the files. On the web, the page downloads the two manifests and the shards of the app domain before this step.
2. The runtime reads `app.json` and creates the window and the engine, and the built-in plugins start.
3. In a release, the content manager looks on a worker thread for required content that the device lacks. It holds the start of the app while that content downloads, behind the [update screen](content.md#the-update-screen). Content that ships with the app is there, so the app goes on in the next frames.
4. The autoloads of `app.json` load in order, and each one gets `start`.
5. The file `source/main.lua` runs, and the engine publishes `appStarted`.
6. Once the app runs, the content manager removes from its store what no retained content uses and downloads the `prefetch` content. With updates on, it checks the channel of the app in the background.

The engine decodes no asset of `content/` on its own. The only one it may load is the splash logo, which the update screen draws when it shows and the logo is on the device. Every other asset enters memory only when the app asks for it: through `assets.load`, `assets.loadAsync`, a preload group, the `load` hook of a scene that preloads, or another asset that depends on it, such as the images of a map or an atlas. Required assets, prefetched assets once they arrived and the assets of an installed pack are on the device, and they stay out of memory until then. Being on the disk never means being in memory.

## Autoloads

An autoload is a Lua module of the app that loads before the first scene and lives for the whole app. It receives the callbacks of the [lifecycle](../core/lifecycle.md#autoloads): `start`, `event`, `fixedUpdate`, `update`, `render`, `renderUi` and `stop`. It holds what every scene shares, such as the profile of the player, the music or a connection.

```json
{
    "name": "My Game",
    "identifier": "com.example.mygame",
    "autoload": ["state.player-data"]
}
```

```lua
-- The file `source/state/player-data.lua`.
local storage = require('haylen.storage')

local playerData = {coins = 0}

function playerData:start()
    if storage.exists('profile.json') then
        self.coins = storage.readJson('profile.json').coins
    end
end

function playerData:stop()
    storage.writeJson('profile.json', {coins = self.coins})
end

return playerData
```

```lua
-- Any module reaches the autoload by its name or by requiring its module.
local haylen = require('haylen')

local playerData = haylen.autoloads.playerData
playerData.coins = playerData.coins + 10
print(require('state.player-data') == playerData)
```

The autoloads of `app.json` load in their order before `source/main.lua`, after the plugins started and after the required content is on the device. The function `haylen.autoload(module)` adds one while the app runs. A restart, such as `content.apply()` or a changed `app.json` in development, stops them from the last to the first and loads them again in a fresh Lua state.

In a release an autoload is Lua bytecode in the encrypted app domain, like every other module. So it belongs to the build of the app: it ships with it, never downloads, and a content update never changes it. Its `start` may load required assets with `assets.load`. An asset that downloads later loads with `assets.loadAsync`, since a synchronous load never waits for the network.

## Loading an asset

The function [`assets.load(path)`](../lua-api/assets.md#assetsloadpath-type-options) loads an asset at once on the frame thread and returns it. It reads the file, which in a release authenticates, decrypts and decompresses the chunks of that file alone. Then it decodes the file and creates its GPU resources. A synchronous load never waits for the network. So an asset that is not on the device yet, such as an `onDemand` asset that nothing brought, raises an error that tells you to use `loadAsync` or a preload group, and so do `assets.json`, `assets.text` and `assets.bytes`. In development the same assets raise the same error, so the mistake shows long before a release.

The function [`assets.loadAsync(path)`](../lua-api/assets.md#assetsloadasyncpath-type-options) returns a promise and does the work away from the frame thread.

1. An asset that is not on the device downloads first. The content manager fetches the encrypted records of its chunks, verifies each one and keeps it in the local store, as [downloads](content.md#downloads) describes.
2. The I/O pool reads the file, which in a release reads, authenticates, decrypts and decompresses its chunks.
3. The task pool decodes it, such as an image into pixels or a map into its data.
4. The frame thread creates its GPU resources within the upload budget of the frame, and the promise resolves.

A failed download rejects the promise with its message, and `:await()` returns `nil` and the message. The upload budget is the time of each frame spent creating GPU resources, which [`assets.setUploadBudget`](../lua-api/assets.md#assetssetuploadbudgetseconds) changes. A larger budget shortens a loading screen, since more assets finish per frame, and it makes those frames longer, so it suits a screen that a transition covers. On the web, which runs on one thread, the jobs of the pools run inside the frame between the other work.

```lua
local async = require('async')
local assets = require('haylen.assets')

async.spawn(function()
    local map, err = assets.loadAsync('world/asia/maps/tokyo.tmj'):await()
    if not map then
        print('Tokyo did not load: ' .. err)
        return
    end
    print('Tokyo is ready.')
end)
```

## Preload groups

A preload group names assets that load together in the background, report their progress, stay in memory while the group holds them and leave together. A later `assets.load` of one of them returns the loaded asset at once. Assets of the group that are not on the device download first, in one request that takes the first half of the progress of the group, so a group needs no code for downloads. A group defines no delivery: it only says what decodes into memory together.

```json
{
    "groups": {
        "boot": ["ui/", "fonts/ui.ttf", "audio/ui/"],
        "menu": [
            "images/menu/",
            {"path": "audio/music/menu.ogg", "options": {"stream": true}}
        ],
        "gameplay": [
            "maps/level-01.tmj",
            "images/characters/",
            {"path": "sprites/hero.json", "type": "atlas"},
            "audio/effects/"
        ]
    }
}
```

```lua
-- The file `source/main.lua` defines the groups of `content/preload.json` once.
local assets = require('haylen.assets')
local scene = require('haylen.scene')

assets.defineGroups('preload.json')
scene.push((require('scenes.menu')))
```

```lua
-- The file `source/scenes/menu.lua`.
local assets = require('haylen.assets')

local menu = {}

function menu:load(context)
    context:preload({'boot', 'menu'}):await()
    self.background = assets.load('images/menu/background.png')
end

function menu:unload()
    assets.unloadGroup('menu')
end

return menu
```

The `boot` group stays loaded for the whole app, while the menu releases its own group when it unloads. The function `assets.unloadGroup` releases the hold of the group, so assets that nothing else holds leave memory, and the group stays defined for a later preload. The [assets reference](../lua-api/assets.md#preload-groups) lists the format of groups and every function and error.

## Scenes

The `load` hook of a scene runs before the scene enters, as a task that can await promises. The call `context:preload(groups)` loads groups as part of it. The load waits for every group, the progress of each group folds into the progress of the load that loading views show, and an asset that fails fails the load. With content that downloads, the same call first brings the assets of the groups that are not on the device. So a scene never knows whether its bytes shipped with the app, arrived earlier, came from a store or download now, as the [optional region](#an-optional-region) shows.

The function `scene.preload(scene, params)` runs the `load` hook of a scene in the background without changing the stack. So the next area loads, and downloads what it lacks, while the player is still in the current one. A later `push` or `replace` of that scene is instant, or waits only for the rest of its load.

```lua
-- The file `source/scenes/map.lua`.
local log = require('haylen.log')
local scene = require('haylen.scene')

local asia = require('scenes.asia')

local map = {}

function map:enter()
    scene.spawn(self, function()
        local loaded, err = scene.preload(asia):await()
        if loaded == nil then
            log.warning('Asia could not load ahead of time: ' .. err)
        end
    end)
end

function map:travel()
    scene.replace(asia, {duration = 0.5})
end

return map
```

A failed download in a scene load fails the load like any asset that fails. The scene unloads, `sceneLoadFailed` is published, and the `onError` of the change receives the message. So a scene that needs content shows its own message, such as an offline device, and offers to try again. The [scene reference](../lua-api/scene.md#load-context) and the [lifecycle guide](../core/lifecycle.md#loading-preloading-and-errors) describe loads, preloads and their errors.

## Delivery

The delivery of an asset says when its encrypted bytes must be on the device. It never says when the asset is decoded into memory.

| Delivery | When the bytes reach the device |
| --- | --- |
| `required` | Before the app starts. They ship with the app, or the engine downloads them behind the update screen when the device lacks them. Every asset is required unless a rule of `app.json` says otherwise. |
| `prefetch` | In the background once the app runs, before anything asks for them, so a later load finds them on the device. The download yields to what the app loads, stops while the app is in the background, goes on when the network comes back and needs room on the device. |
| `onDemand` | Only when the app asks for the asset: through `assets.loadAsync`, a preload group, the `load` hook of a scene that preloads, or `content.prefetch`. Once the bytes arrived they stay, and the asset behaves like a local one. |

A required asset stays on the disk until the app loads it. A prefetched asset costs storage and no memory until a load or a preload decodes it. A synchronous load never waits for the network, and every asynchronous path brings the asset first. Lua modules, `app.json` and plugins are always required and belong to the build of the app.

The `delivery` rules of the `content` section of `app.json` match paths relative to `content/`, where `**` stands for any folders, `*` for any name inside a folder and `?` for one character. The first rule that matches an asset decides it. The key `updates.baseUrl` names the server that `content publish` fills, from which assets download. The [delivery settings](content.md#delivery) list every key with its default and its errors.

### An optional region

A large world keeps a region off the device until the player goes there.

```json
{
    "name": "Large World",
    "identifier": "com.example.largeworld",
    "version": "1.0.0",
    "content": {
        "updates": {"baseUrl": "https://content.example.com/large-world/", "channel": "stable"},
        "delivery": [
            {"match": "world/asia/**", "mode": "onDemand"},
            {"match": "cinematics/4k/**", "mode": "prefetch"}
        ]
    }
}
```

```json
{
    "groups": {
        "asia": ["world/asia/maps/", "world/asia/textures/", "world/asia/audio/"]
    }
}
```

```lua
-- The file `source/scenes/asia.lua`.
local assets = require('haylen.assets')

local asia = {}

function asia:load(context)
    context:preload('asia'):await()
end

function asia:unload()
    assets.unloadGroup('asia')
end

return asia
```

Every asset that no rule matches stays required. The release keeps the files of each delivery in shards of their own. So the app ships the shards of the required assets, and the shards of Asia and of the cinematics stay on the server. When the player reaches Asia and it is not on the device, `context:preload('asia')` does all the work:

1. It downloads the encrypted chunks that the device lacks, verifies them and keeps them in the local store.
2. It reads, decrypts, decompresses and decodes the assets.
3. It creates their GPU resources within the upload budget, and the load finishes.

An app without a server ships its whole release, every delivery included. So every asset is on the device, and the rules change only how its shards are grouped.

### Downloading without loading

The function [`content.prefetch(group)`](../lua-api/content.md#contentprefetchgrouporpaths-progress-options) brings the bytes of a group, or of a list of assets, onto the device without decoding them. So the next area downloads while the player keeps playing, and it costs storage and no memory. A later preload of the group reads the bytes from the device at once and only decodes.

```lua
-- The file `source/scenes/europe.lua`.
local content = require('haylen.content')
local scene = require('haylen.scene')

local europe = {}

function europe:enter()
    scene.spawn(self, function()
        local done, err = content.prefetch('asia', function(progress)
            self.asiaProgress = progress.receivedBytes / math.max(progress.totalBytes, 1)
        end, {owner = self}):await()
        if not done then
            print('Asia downloads when the player goes there: ' .. err)
        end
    end)
end

return europe
```

The function `content.isLocal(path)` tells whether every byte of an asset is on the device, so an app can show which regions are ready.

### Packs that stores install

A rule with `pack` instead of `mode` puts the matching assets into a pack: content that a store installs apart from the app, such as an expansion sold on its own. A pack never holds back the start, never downloads in the background and never comes with an update. Its assets are on the device once its store installed it.

```json
{
    "content": {
        "delivery": [{"match": "expansions/forest/**", "pack": "forest"}]
    }
}
```

The app loads the assets of a pack like any other asset, whichever store delivers it. The runtime finds the shards of an installed pack in `packs/` next to the executable or the bundle on Windows, Linux and macOS, and in every folder that `content.addPackFolder(path)` names while the app runs. A store that delivers packs on request plugs in through its native plugin with `content.addPackSource(fn)`. An asynchronous load of an asset of a pack that is not installed asks each source for the pack first.

An asset of a pack that no source delivers is unavailable. Then `assets.load` raises the error of an asset that is not on the device, and `assets.loadAsync` fails with `storeAssetUnavailable`, unless the app names a server, from which the asset downloads. The [packs](content.md#packs) section of the content guide describes the sources of packs, the asset packs of Google Play and the asset packs of Background Assets.

### Development

In development the app plays its folder and downloads nothing, and it has no updates. The delivery rules still apply: a `prefetch` or `onDemand` asset is not on the device until `assets.loadAsync`, a preload group or `content.prefetch` brings it, which takes one frame. So a synchronous load of such an asset fails in development as it may in a release, where the prefetch may not have finished yet, and the mistake shows long before a release. The assets of packs are on the device in development.

## Streaming music

The option `stream = true` of a sound keeps only a reader of its file, so a track of any length plays in the same bounded memory. It works the same in a release: the reader decrypts and decompresses only the chunks around the cursor of each voice. So a streamed track never exists decrypted as a whole, in memory or on disk, as [streaming](content.md#streaming) describes.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local music = assets.load('audio/music/menu.ogg', 'sound', {stream = true})
audio.playMusic(music)
```

Preload groups take the same option, `{"path": "audio/music/menu.ogg", "options": {"stream": true}}`, so music loads as a stream behind a loading screen. The [audio guide](../audio/audio.md#sounds) compares decoded and streamed sounds.

## Caches and memory

Several layers keep content, and each one lives in another place.

| Layer | What it holds | Where | Lifetime |
| --- | --- | --- | --- |
| Asset cache | Decoded assets by type, path and options, so a second load of the same asset returns the same object. | Memory. | While anything holds the asset, such as a Lua variable, another asset or a preload group. The cache holds only weak references. |
| GPU resources | The textures, buffers and atlases of decoded assets. | The GPU. | Released with the last holder of their asset, and destroyed by the device after the frame is submitted. |
| Release reading | The decrypted catalogs of both domains, the decrypted indexes of at most 16 open shards of each manifest, and the last decrypted chunk of each open file reader, at most 4 MiB. | Memory. | Catalogs for the whole run, shard indexes while their shards stay among the 16 used last, and a chunk until the reader moves on or ends, when it is wiped. |
| Shards of the app | The encrypted shards that ship with the app, or that a store installed as packs. | The disk, inside the app or next to it. | As long as the app is installed. |
| Local content store | The encrypted records that downloads brought, sealed as they arrived, with signed manifests and encrypted shard indexes. | The disk, in a folder of the app apart from its saves, as the [local store](content.md#local-store) lists per platform. | Until no retained content uses them, when the next start collects them. |
| Browser store | The local store of a web page, with the records that it read in. | The private file system of the origin, and memory for the records that a visit read in. | The required content stays in memory for the whole visit, and other records within 64 MiB, the ones read least recently going first. The files stay until the browser clears them. |
| Build cache | Sealed records of earlier builds. | `build/apps/<app>-<hash>/content-cache/` of the build machine. | Until deleted, which only costs time. |

When the platform reports low memory, the engine removes the cache entries of assets that nothing holds, whose memory was already freed with their last holder. Then it publishes `appLowMemory`, which is the moment for the app to unload preload groups that it can load again. The encrypted bytes on the disk are untouched, since they take storage and no memory, and the memory of the release reading stays within its bounds.

Storage is a separate matter. The system may clear the store of tvOS, which lives in `Caches`, and a browser may clear the store of a page. The next load of an asset whose bytes went then downloads them again, and required content downloads behind the update screen at the next start.

## Platforms and stores

Every platform ships the same protected release, built for its profile, and the code of the app never branches on the platform or the store.

| Platform | What ships | How content arrives later |
| --- | --- | --- |
| Apple | The release in `app/` of the bundle resources, and the bootstrap compiled into the app. | From the server of `updates.baseUrl`. On macOS also from `packs/` next to the bundle. On iOS and tvOS 26 and later, the named packs come from the asset packs of Background Assets that the App Store hosts, through a plugin such as `background-assets`. |
| Android | The release in the assets of the APK or the App Bundle, with its shards uncompressed, a release too large for the base module of Google Play in install-time asset packs, and `libhaylen-app.so` with the bootstrap for every ABI. | From the server of `updates.baseUrl`, and the named packs from `fast-follow` and `on-demand` asset packs of Google Play, through a plugin such as `play-asset-delivery`. |
| Steam | The depot of the app with the executable or the bundle and its release, and a depot per pack. | Through SteamPipe, which updates changed files, and from the server of `updates.baseUrl` when the app names one. |
| Web | The runtime of the app for WebGPU and WebGL2 and the whole release in `app/` of the site. | By byte range from `app/` of the site, into the browser store. |

The release ships inside the app, only its required part when the app names a server. The named packs of the content may reach the app from a store. On Android, the asset packs of Google Play deliver them through a plugin that declares `assetPacks`, such as `play-asset-delivery`. On iOS and tvOS 26 and later, Background Assets delivers them through a plugin such as `background-assets`. Each downloads a pack with the install or when the app loads one of its assets, while older systems take the pack from the content server.

Stores limit the size of an app, and the release builds follow those limits on their own. An Apple release build warns when it passes the 4 GB uncompressed that the App Store accepts for iOS and tvOS apps. An Android release whose shards do not fit next to the code in the 500 MB of the base module of Google Play moves its content shards into install-time asset packs. The [distribution guide](../tools/distribution.md#release-builds) describes every limit of the stores. Content beyond them goes on the content server with `onDemand` or `prefetch` rules, or into packs.

### Apple

```sh
haylen prepare ~/apps/my-game --platform ios --config Release
```

The command `prepare` writes the release of the `apple` profile into `haylen/app/` of the Apple project and the bootstrap into `haylen/HaylenBootstrap.cpp`, so the project builds and archives in Xcode. Every target of the project, iOS, tvOS, Mac Catalyst and macOS, shares the release.

```text
MyGame.app/
  MyGame                       The executable, with the bootstrap of the app compiled in.
  app/app.hmanifest            The signed manifest of the app domain.
  app/content.hmanifest        The signed manifest of the content domain.
  app/<id>.hpak                The shards that ship: all of them, or for an app with a server those of the app domain and the required assets.
```

On macOS the release sits in `Contents/Resources/app/`. The app plays it from the bundle and keeps its downloads in `Application Support/<identifier>-content`, or in `Caches/<identifier>-content` on tvOS. On macOS it reads packs from `packs/` next to the bundle.

An app with a plugin that delivers its named packs from the App Store, such as `background-assets`, also gets an asset pack of Background Assets for each pack. The developer uploads those to App Store Connect with the build. The plugin adds the downloader extension, the App Group and the keys of the `Info.plist` that the system needs.

```text
build/apps/my-game-1a2b3c4d/release/apple.asset-packs/
  chapter-2.aar                The pack "chapter-2", which downloads when the app loads one of its assets.
  intro.music.aar              The pack "intro.music", which the plugin lists in "prefetch" and downloads with the install.
MyGame.app/Extensions/Downloads.appex   The downloader extension of the plugin.
```

### Google Play

```sh
haylen android-key ~/apps/my-game
haylen run ~/apps/my-game --platform android --config Release
haylen prepare ~/apps/my-game --platform android --config Release
```

The command `run` builds, inspects and installs the release APK, signed with the upload key that `android-key` created. The command `prepare` writes the release of the `android` profile into `haylen/assets/app/` and the library of the app into `haylen/jniLibs/`. Then the Gradle project of the app builds the App Bundle for Google Play, such as with its `bundleRelease` task or the signed bundle of Android Studio.

```text
assets/app/app.hmanifest       The signed manifest of the app domain.
assets/app/content.hmanifest   The signed manifest of the content domain.
assets/app/<id>.hpak           The shards that ship, stored uncompressed, which the runtime reads in place.
assets/haylen-plugins.json     The plugins with an Android part and their values.
lib/<abi>/libhaylen-app.so     The Lua player with the bootstrap of the app, without symbols.
```

The template keeps `noCompress` of `hpak`, which `check --config Release` verifies, and downloads stay in `no_backup/<identifier>-content` of the data folder of the app. Haylen picks the asset packs on its own, so you never split the release by hand.

```text
haylen/asset-packs/haylencontent1/src/main/assets/app/<id>.hpak   The content shards of a release larger than the share of the base module that the code leaves it, 384 MiB, in install-time asset packs of at most 1 GiB, which install with the app.
haylen/asset-packs/chapter_2/src/main/assets/<id>.hpak            The pack "chapter-2" as an on-demand asset pack, with a plugin that declares "assetPacks", such as "play-asset-delivery".
haylen/asset-packs/intro_music/src/main/assets/<id>.hpak          The pack "intro.music" as a fast-follow asset pack, which the plugin lists in "fastFollow".
```

The manifests and the shards of the app domain, the code of the app, always stay in the base module. The shards of Android releases aim for 256 MiB, so each fits its pack, and the App Bundle of `bundleRelease` holds every pack. The command `run` installs a release with install-time packs from the universal APK of the bundle. It installs a release with packs that Google Play delivers later with bundletool, in the local testing mode of Google Play, as the [distribution guide](../tools/distribution.md#release-builds) describes. The folders `<pack>/assets/` of the App Bundle show what each pack holds.

### Steam

```sh
haylen steam ~/apps/my-game --platform windows --app-id 1000 --depot windows=1001 --depot forest=1002 --branch beta
steamcmd +login <account> +run_app_build build/apps/my-game-1a2b3c4d/steam/scripts/app_build.vdf +quit
```

The command builds the release on a machine of the platform, inspects it and stages the depots. Each depot needs its ID from Steamworks, with the platform as one name and each pack as another. The option `--branch` sets the branch that the build goes live on, such as `beta` for testers.

```text
build/apps/my-game-1a2b3c4d/steam/
  content/windows/my-game.exe              The executable of the app with its bootstrap.
  content/windows/app/app.hmanifest
  content/windows/app/content.hmanifest
  content/windows/app/<id>.hpak            The shards that ship: every one but those of packs, or for an app with a server those of the app domain and the required assets.
  content/forest/packs/<id>.hpak           The depot of the pack "forest", which installs into packs/ next to the executable.
  scripts/app_build.vdf
  scripts/depot_build_1001.vdf
  scripts/depot_build_1002.vdf
```

An app without a content server ships its whole release except its packs in the depot of the app. So Steam delivers all of its content, and the app needs no server of its own. An update is a new build. The release reuses the shards of the release before it and keeps every unchanged shard byte for byte under the same name. So SteamPipe uploads and downloads only the shards that changed and the two manifests, and a new build may carry new Lua bytecode, since the code belongs to the build. A player who does not own a pack never receives its depot.

### Web

```sh
haylen prepare ~/apps/my-game --platform web --config Release
haylen run ~/apps/my-game --platform web --config Release
```

The command `prepare` makes the site in `build/apps/<app>-<hash>/web/`, and `run` serves it on this machine with byte ranges, as a content delivery network does. The site holds the runtime of the app for WebGPU and for WebGL2, linked with the bootstrap of the app, and the release in `app/`.

```text
index.html, loader.js, loader.css, app.js, config.json, splash.png
webgpu/haylen.js, webgpu/haylen.wasm, webgpu/haylen-audio-worklet.js, webgpu/haylen-content-store.js
webgl2/...                     The same files for WebGL2.
app/app.hmanifest
app/content.hmanifest
app/<id>.hpak                  Every shard of the release.
```

The site holds no `app.zip`, no `source/`, no `content/` and no Lua text. The page downloads only what the app needs, by byte range from `app/`, so the page of a large game starts after a download of a few megabytes, as [the web](content.md#the-web) section of the content guide explains. The server serves `app/` by byte range with a strong `ETag` and long cache lifetimes, as [serving a release](content.md#serving-a-release) lists with the headers.

## Updates

An app updates its content in two ways.

- A new build of the app through its store ships a new release. That release reuses the shards of the release before it, so the patch of a store that compares files holds only the shards that changed.
- An app that names a server in `content.updates` also takes new content from its channel without a new build. When updates are on, it checks the channel when it starts, downloads newer content in the background and plays it from the next start, or at once with `content.apply()`. A channel publishes only the content domain, so new Lua code always comes with a new build.

```sh
# Keep the release that shipped, to compare later releases with it.
cp -R build/apps/my-game-1a2b3c4d/release/android ~/releases/my-game-1.0.0-android

# After changing assets, build the release again and see what an installed app downloads.
haylen content build ~/apps/my-game --platform android
haylen content diff ~/apps/my-game ~/releases/my-game-1.0.0-android build/apps/my-game-1a2b3c4d/release/android

# Publish the next generation of the channel into the publication tree, then remove what no channel reaches.
haylen content publish ~/apps/my-game --platform android --channel stable --output ~/cdn/my-game
haylen content compact ~/apps/my-game ~/cdn/my-game --keep 2
```

The command `content diff` shows what changed between two releases. Here a small app changed one line of a data file and made its 2 MiB track load on demand:

```text
The app domain adds 1 chunks of 642 bytes, reuses 1 chunks of 2231 bytes and drops 1 chunks of 487 bytes, with 1 new shards and 0 added, 1 changed and 0 removed files.
  Changed "app.json".
The content domain adds 1 chunks of 15 bytes, reuses 3 chunks of 2158815 bytes and drops 1 chunks of 15 bytes, with 1 new shards and 0 added, 1 changed and 0 removed files.
  Changed "content/data/levels.json".
  Delivered "content/music/theme.ogg" as "onDemand" instead of "required".
An app that holds the earlier release downloads about 2604 bytes to reach the later one.
```

The second build kept both shards of the first one byte for byte, the 2 MiB shard of the track among them. It added a shard of 367 bytes for the new data file and one of 656 bytes for the new `app.json`. So a store that patches files by their bytes, such as Steam, ships only the two small shards and the manifests. The track kept its record where it was, and its new delivery lives in the catalog, so nothing of it downloads again.

The command `content publish` adds the next generation to the channel and moves the channel pointer last, as [publication trees](content.md#publication-trees) describes. Upload the tree to the server with any tool that copies files, the packs and manifests before `channels/`, so no app reads a pointer to files that are not there yet. An installed app downloads only the chunks it lacks, verifies every one, and switches to the new content as one transaction. So an interrupted update leaves the content that it plays as it was, as [updates](content.md#updates) describes. An app with `enabled` set to `false` checks, downloads and applies updates itself with [`haylen.content`](../lua-api/content.md#updates).

## Inspecting a release

```sh
haylen content build ~/apps/my-game --platform ios
haylen content verify ~/apps/my-game --platform ios
haylen content inspect ~/apps/my-game --platform ios
haylen content inspect ~/apps/my-game --platform ios --chunks
haylen run ~/apps/my-game --platform ios-simulator --config Release
haylen check ~/apps/my-game --platform ios-simulator --config Release
```

The commands `content verify` and `content inspect` check a release and list what it holds, as [the content tool](content.md#the-content-tool) describes. The command `check --config Release` inspects the last release build of a platform. It fails when the built app holds a `.lua` file, the index of a development package, a file of the package as it is under any name, other files in the release folder than manifests and shards, symbol files, or a key of the app in any form. The command `run --config Release` makes the same inspection after every build and stops before it launches. So a continuous integration job that runs `check --config Release` proves that no raw `source/` or `content/` ships.

## Keys

The keys of an app live in its key folder on the machine that builds it, never in a repository, a project or a build folder. The folder holds the content keys that encrypt and the signing key that signs, and only its owner can read it. The [key folders](content.md#key-folders) section of the content guide gives its place on each system and lists its files.

The first release build creates the keys and warns that the folder needs a backup. Every later release and update of the app must be built with the same keys, or the installed apps cannot read it. Back the folder up as a secret, such as in an encrypted archive or a password manager. The command `haylen content keys <app>` shows its key IDs and its public key without printing a key. The command `haylen content keys <app> --rotate` adds a content key for later builds, while the earlier keys stay for the content that installed apps hold.

Continuous integration receives a copy of the key folders as a secret and names their parent folder with `HAYLEN_KEYS_DIR`. There `haylen.py` never creates keys, so a missing folder stops the build instead of producing a release that no installed app can read.

```yaml
env:
  HAYLEN_KEYS_DIR: ${{ runner.temp }}/haylen-keys
steps:
  - run: |
      mkdir -p "$HAYLEN_KEYS_DIR"
      echo "${{ secrets.HAYLEN_CONTENT_KEYS }}" | base64 --decode | tar -x -C "$HAYLEN_KEYS_DIR"
  - run: python3 haylen.py prepare my-game --platform linux --config Release
  - run: python3 haylen.py check my-game --platform linux --config Release
```

On a Mac, the command `tar -c -C ~/Library/Application\ Support/Haylen/keys com.example.mygame | base64` prints the text of that secret. A release build compiles the content keys into the binary of the app, sealed so that the binary never holds them in plain form. No key sits in a resource, a manifest or `app.json`, as [keys in the app](content.md#keys-in-the-app) describes.

## Performance

Protection adds work to every read of a release, and that work runs where the read runs. For each chunk of a file, the read authenticates and decrypts the record, decompresses it when it was compressed, and checks its digest against the catalog. Asynchronous loads, preload groups and scene loads do this work on the I/O pool and decode on the task pool. So the frame thread only creates GPU resources within its upload budget. The synchronous calls `assets.load`, `assets.json`, `assets.text`, `assets.bytes` and `require` do the work on the frame thread, like the read they make in development, so large assets load asynchronously.

A load reads only the chunks of its file, never a whole shard, and reads in order decrypt each chunk once. A small read at a random place costs the whole chunk that holds it. The [performance](content.md#performance) section of the content guide lists the measurements of `haylen bench --suite content`.

The release also saves work:

- Lua modules load as bytecode without being parsed, about three times faster than from their text.
- Chunks that several files share are stored once.
- Compression makes shards and downloads smaller where the bytes compress.
- An update downloads only the chunks that changed, and the range planner joins many small records into few requests.

A release stores every asset with the bytes of its file and converts nothing besides Lua. So a map, an image or a JSON file decodes in a release exactly as in development, and shaders reach both as the `.shader` files that `haylen shaders` compiles. A streamed sound reads only the chunks around the cursor of each voice, as [streaming music](#streaming-music) describes.

## Errors in a release

The bytecode of a release keeps its debug information, so an error reads as in development. The error shows the message, the module, its line and the names of locals, such as `attempt to index a nil value (local 'enemy')` in `source/scenes/level.lua`, and the stack of the failing call. The error screen of a release shows them without the excerpt of source lines, since a release holds no Lua text. A web page receives the same report in `Module.haylen.onError`, as [errors and stack traces](../core/lua.md#errors-and-stack-traces) describes.

## Security

A release keeps the files of an app away from the common ways of taking them: browsing a bundle or an APK, extracting it with archive tools, copying its images, data and scripts, crawling named assets, and changing content without the signing key. It does not protect against someone who controls the device and reads memory or the binary. Each app has its own keys, so recovering them exposes only that app. The [security](content.md#security) section of the content guide draws the exact line.

## From C++

C++ code reaches the same loading through the engine.

- The call `engine.getAssets()` returns the `assets::Manager`, with `load`, `loadAsync`, `defineGroups`, `preload`, `unloadGroup` and `setUploadBudget`.
- The load hook of a C++ scene preloads groups with `core::SceneLoad::preload`, and `engine.getScenes()` preloads scenes with `SceneManager::preload`.
- The call `engine.getContent()` returns the `content::ContentManager`, with `isLocal`, `acquire`, `addPackFolder`, `addPackSource`, `check`, `download` and `apply`, as [the content manager](content.md#the-content-manager) describes.
