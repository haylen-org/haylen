# 📖 `haylen.content`

The module `haylen.content` tells which content the app plays and which of its assets are on the device, brings assets down ahead of time without loading them, and checks, downloads and applies the updates of the content. Most apps never need it: [`haylen.assets`](assets.md) brings an asset that is not on the device before it reads it, and the engine downloads the content that an app needs to start, the prefetched content and its updates by itself. The module serves apps that show the state of their content, bring a region of their world down while the player is elsewhere, or decide themselves when to update. The [loading guide](../content/loading.md#delivery) explains deliveries and development with examples, and the [content guide](../content/content.md) explains how releases, downloads and updates work.

```lua
local content = require('haylen.content')
```

In development the app plays its folder, without downloads or updates, while the delivery rules of `app.json` still apply, as [development](../content/loading.md#development) describes.

## State

### content.isProtected()

Returns `true` when the app plays a protected release, as a release build does, and `false` in development.

```lua
local content = require('haylen.content')

print(content.isProtected() and 'A release.' or 'Development.')
```

### content.generation()

Returns the generation of the content that plays: `1` for the content that ships with a release, the generation of its channel for an update that the app installed, and `0` in development.

```lua
local content = require('haylen.content')

print('Content generation ' .. content.generation() .. '.')
```

### content.channel()

Returns the update channel that `content.updates` of `app.json` names, such as `'stable'`, or `nil` for an app without a content server.

```lua
local content = require('haylen.content')

if content.channel() == 'beta' then
    print('This app takes the content of testers.')
end
```

### content.status()

Returns what the content does now:

| Status | Meaning |
| --- | --- |
| `'idle'` | Nothing runs. |
| `'checking'` | It asks the channel for an update. |
| `'downloading'` | It brings the content that the app needs to start, or an update. |
| `'ready'` | An update is installed and plays from the next start. |
| `'failed'` | An error happened, which `content.error()` describes. |

```lua
local content = require('haylen.content')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        self.showUpdateBadge = content.status() == 'ready'
    end,
})
```

### content.progress()

Returns the progress of the download of the required content or of an update, as a table with `receivedBytes` and `totalBytes`.

```lua
local content = require('haylen.content')

local progress = content.progress()
print(string.format('%d of %d bytes', progress.receivedBytes, progress.totalBytes))
```

### content.error()

Returns the last failure as a table, or `nil`.

| Field | Meaning |
| --- | --- |
| `code` | The kind of failure, such as `'networkOffline'`, `'networkTimeout'`, `'insufficientStorage'`, `'quotaExceeded'`, `'rangeUnsupported'`, `'manifestIncompatible'` or `'manifestRollbackRejected'`. The [content guide](../content/content.md#errors) lists every code, with a capital first letter in C++. |
| `message` | The sentence that explains it. |
| `retryable` | Whether trying again may succeed without a new build of the app, such as once the network comes back or room is freed. |
| `httpStatus` | The status that the server answered, when the failure was one. |
| `requiredBytes`, `availableBytes` | The room that an update needs and the room that is free, when the device lacked it. |
| `path` | The file of the package that the app read, such as `content/levels/forest.json`, when a read failed. |
| `manifestId`, `packId`, `contentId`, `storedId`, `keyId` | The hexadecimal IDs of the manifest, the pack, the plain content, the stored record and the content key involved, when the failure knows them, such as the key of an update that no key provider of the app gives. |
| `platform` | The content profile of the app, when a manifest was made for another one. |
| `platformMessage` | What the system or the browser said, such as the reason of a refused write. |

The promises of `content.prefetch`, `content.check` and `content.download` reject with the same table. Their `:await()` gives `nil`, the message and the table, which also holds the message in `error`. The second argument of a `catch` handler is the table. Such a rejection never stops the app when nothing awaits it.

```lua
local async = require('async')
local content = require('haylen.content')

local failure = content.error()
if failure and failure.code == 'insufficientStorage' then
    print('Free ' .. (failure.requiredBytes - failure.availableBytes) .. ' bytes to update.')
end

async.spawn(function()
    local done, message, prefetchFailure = content.prefetch({'levels/forest.json'}):await()
    if not done and prefetchFailure.retryable then
        print('The level downloads later: ' .. message)
    end
end)
```

### content.update()

Returns the update that the last check found, or `nil`. The update is a table:

| Field | Meaning |
| --- | --- |
| `generation` | The generation of the update. |
| `downloadBytes` | The bytes that installing it downloads. |
| `reusedBytes` | The bytes of its required content that the device already holds, which installing it never downloads again. |

```lua
local content = require('haylen.content')

local update = content.update()
if update then
    print(string.format('Generation %d downloads %d bytes and reuses %d.', update.generation, update.downloadBytes, update.reusedBytes))
end
```

### content.isLocal(path)

Returns whether every byte of an asset is on the device, by its path relative to the `content` folder, so loading it never waits for a download. An asset that does not exist counts as on the device, and loading it reports that it is missing.

On the web the content store lives in the browser. There an asset counts once the page read its bytes in during the visit. The calls `assets.loadAsync`, `content.prefetch` and a preload group read them in without a download when the store holds them, and the required content is always read in before the app starts.

```lua
local content = require('haylen.content')

if not content.isLocal('world/asia/maps/tokyo.tmj') then
    print('Tokyo downloads when the player goes there.')
end
```

### content.isGroupLocal(name)

Returns whether every asset of the [preload group](assets.md#preload-groups) `name` is on the device, so preloading it reads the bytes from the device at once and never waits for a download. A group that is not defined raises `The asset group "<name>" is not defined.`.

```lua
local assets = require('haylen.assets')
local content = require('haylen.content')

assets.defineGroups('groups.json')
if not content.isGroupLocal('asia') then
    content.prefetch('asia')
end
```

### content.stats()

Returns counters of the content for debugging, which the Content section of the [debug overlay](debug.md) shows too.

| Field | Meaning |
| --- | --- |
| `generation` | The generation of the content that plays. |
| `catalogFiles` | The files that the content holds. |
| `storedRecords`, `storedBytes` | What the local store of downloads holds. |
| `openShards` | The shard files open now. |
| `decodedChunks`, `decodedBytes` | The chunks read since the content was mounted, and their plain bytes. |
| `decodeMilliseconds` | The time that reading, decrypting, decompressing and checking those chunks took, on every thread. |
| `activeReads` | The chunk reads that run now. |
| `downloadedBytes`, `requests` | The bytes downloaded and the requests made since the app started. |
| `resumedRecords` | The downloads that went on from where an earlier one stopped. |
| `pendingDownloads` | `1` while anything downloads. |
| `wantedRecords` | The records that downloads still wait for. |
| `activeTransfers` | The range requests that run now. |
| `downloadRate` | The bytes per second over the last half second. |
| `reusedBytes` | The bytes that the offered update reuses. |
| `installedShards` | The shards that the device holds as files, installed with the app or by a store. |
| `remoteShards` | The shards whose records the local store or the server brings instead. |
| `missingChunks` | The chunks of the content that the device holds in neither. |
| `pendingPrefetch` | The prefetched chunks among the missing ones. |

A survey of the content on a worker counts the last four fields, at most once a second while something reads the counters. So the first call returns them as zero.

```lua
local content = require('haylen.content')

local stats = content.stats()
print(string.format('%d files, %d shards open, %.1f KiB/s', stats.catalogFiles, stats.openShards, stats.downloadRate / 1024))
```

## Bringing assets

### content.prefetch(groupOrPaths, progress, options)

Brings assets onto the device without loading them, and returns a promise that resolves to `true` once every one of them is there. A later `assets.load`, `assets.loadAsync` or preload of the group reads the bytes from the device at once and only then decodes the assets into memory, so prefetching costs storage and no memory.

- The argument `groupOrPaths` is the name of a [preload group](assets.md#preload-groups), whose folders count with every asset in them, or a list of asset paths.
- The optional `progress` function receives a table with `receivedBytes` and `totalBytes` as bytes arrive.
- The optional `options` table takes `owner`, which ends the progress function with its owner. Any other key raises `Unknown option "<key>".`.

The assets download as foreground work, ahead of the prefetched content of `app.json`. In development they are there in the next frame. The promise rejects with the failure, its message and the table of [`content.error`](#contenterror), such as for a device that is offline, or one without room for the assets and the margin that the store keeps free. An asset that does not exist raises `The asset "<path>" does not exist.`, and a group that is not defined raises `The asset group "<name>" is not defined.`.

```lua
local async = require('async')
local content = require('haylen.content')

async.spawn(function()
    local done, err = content.prefetch('asia', function(progress)
        print(progress.receivedBytes .. ' of ' .. progress.totalBytes .. ' bytes')
    end):await()
    print(done and 'Asia is on the device.' or err)
end)
```

### content.addPackFolder(path)

Reads the shards of a pack from a folder where a store installed them, while the app runs. The assets of the pack load from then on, without a restart. A native plugin of a store that delivers packs on request, such as an asset pack that a store downloads when the player buys an expansion, calls it with the folder that the store reports. Every shard is checked against the signed manifest of the release, so a folder never brings content that the release does not name. In a release, a path that is no folder raises `The pack folder "<path>" does not exist.`. In development the folder of the app holds every pack, so the call does nothing.

```lua
local content = require('haylen.content')

-- The plugin of the store answers with the folder of the pack that it installed.
local function packInstalled(folder)
    content.addPackFolder(folder)
    print(content.isLocal('expansion/island.json'))
end
```

### content.addPackSource(fn, options)

Adds a source of packs and returns a [`Connection`](signal.md#connection) that removes it. When the app loads an asset of a pack that the device lacks, with `assets.loadAsync`, `assets.preload` or `content.prefetch`, the engine asks each source for the pack before the content server, in the order the sources were added. So the code that loads the asset never knows which store delivers it. In development the folder of the app holds every pack, so no source is asked.

The function `fn` runs as a task with the name of the pack and a progress function, which takes a table with `receivedBytes` and `totalBytes`. It returns one of these:

- The folder that holds the shards of the delivered pack. The folder joins the release as `content.addPackFolder` adds it.
- `nil` when the source does not deliver the pack, which passes the pack on to the next source.
- A promise of either.

A load whose pack no source delivers comes from the content server, or fails with `storeAssetUnavailable` in an app without one. The table `options` takes `owner`, whose end removes the source and stops its tasks, and any other key raises `Unknown option "<key>".`. A source that is removed while it delivers counts as one that delivers nothing. The [errors](#errors) section lists what a failing source reports.

```lua
local content = require('haylen.content')
local store = require('store-packs')

-- The Lua part of a store plugin hands every pack request to its native part, which answers with the folder where the store installed the pack.
local plugin = {}

function plugin:start()
    content.addPackSource(function(pack, progress)
        return store.download(pack, progress)
    end, {owner = self})
end

return plugin
```

## Updates

An app with a content server and `enabled` updates checks its channel when it starts, downloads newer content in the background and plays it from its next start, so most apps never call these functions. An app whose `content.updates` sets `enabled` to `false` decides itself when to check and download, as [updates](../content/loading.md#updates) describes.

### content.check()

Asks the channel for newer content and returns a promise. It resolves to a table with `generation` and `downloadBytes`, or to `nil` when the app plays the newest content. It rejects with the failure, its message and the table of [`content.error`](#contenterror), such as `manifestRollbackRejected` for a server that offers an older generation, or `manifestIncompatible` for content that needs a newer build of the app.

In development it raises `The app plays its folder in development, which has no updates. Build it in the Release configuration to check for updates.`. In a release without a content server it raises `The app names no content server, so it has no updates. Name one in "content.updates" of "app.json".`.

```lua
local async = require('async')
local content = require('haylen.content')

async.spawn(function()
    local update, failure = content.check():await()
    if update then
        print('Generation ' .. update.generation .. ' is ready to download.')
    elseif failure then
        print(failure)
    end
end)
```

### content.download(progress, options)

Installs the update that the last check found, checking first when none was found, and returns a promise. The promise resolves to `true` once the update is on the device, and it plays from the next start. When the channel offers nothing newer, the promise also resolves to `true` and nothing is installed, so `content.status()` reads `'ready'` only when an update waits.

The device must first have room for the download and a margin, or the promise rejects with `insufficientStorage`. The `progress` function and the `options` work as in [`content.prefetch`](#contentprefetchgrouporpaths-progress-options). A download that stops resumes where it stopped the next time, and the content that plays never changes until the whole update is verified. Without a content server, or in development, it raises the errors of `content.check`.

```lua
local async = require('async')
local content = require('haylen.content')

async.spawn(function()
    local done, err = content.download(function(progress)
        print(progress.receivedBytes .. ' of ' .. progress.totalBytes .. ' bytes')
    end):await()
    if done and content.status() == 'ready' then
        print('The update plays from the next start.')
    elseif not done then
        print(err)
    end
end)
```

### content.apply()

Starts the app again on the update that `content.download` installed, at the end of the frame. It raises `No content update is ready to apply. Download one first.` when none is ready.

```lua
local async = require('async')
local content = require('haylen.content')

async.spawn(function()
    local update = content.check():await()
    if update then
        print('Downloading ' .. update.downloadBytes .. ' bytes.')
        if content.download():await() then
            content.apply()
        end
    end
end)
```

### content.cancel()

Stops the check or the download that runs, and its promise rejects with `networkCancelled`. The bytes that arrived stay for the next attempt.

```lua
local content = require('haylen.content')
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if content.status() == 'downloading' and input.keyPressed('escape') then
            content.cancel()
        end
    end,
})
```

## Events

The content publishes these events on [`haylen.events`](events.md#engine-events).

| Event | Data |
| --- | --- |
| `contentUpdateAvailable` | `generation` and `downloadBytes` of the update that a check found. |
| `contentUpdateReady` | `generation` of the update that is installed and plays from the next start. |
| `contentUpdateFailed` | `code` and `message` of a check or a download that failed. |
| `contentRecovered` | `manifest`, the ID of downloaded content that could not play again, and `reason`, once the app fell back to earlier content when it started. |

```lua
local content = require('haylen.content')
local events = require('haylen.events')

events.on('contentUpdateReady', function()
    print('The new content plays from the next start, or now with content.apply().')
end)
```

## Errors

These messages are raised by the functions, or reach the promise of a load through a source of packs.

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | An options table has a key other than `owner`. |
| `An owner must be a table or a userdata, not <type>.` | The option `owner` is another type. |
| `The asset "<path>" does not exist.` | The function `content.prefetch` names an asset that the package lacks. |
| `The asset group "<name>" is not defined.` | The functions `content.prefetch` or `content.isGroupLocal` name a group that was never defined. |
| `The pack folder "<path>" does not exist.` | The function `content.addPackFolder` names a path that is no folder, in a release. |
| `The app plays its folder in development, which has no updates. Build it in the Release configuration to check for updates.` | The functions `content.check` or `content.download` ran in development. |
| `The app names no content server, so it has no updates. Name one in "content.updates" of "app.json".` | The functions `content.check` or `content.download` ran in a release without a content server. |
| `No content update is ready to apply. Download one first.` | The function `content.apply` ran before an update was installed. |
| `The source of packs must return the folder of the pack, nil or a promise of either.` | A function of `content.addPackSource` returned another value. |
| `The source of packs failed to deliver the pack "<pack>". <reason>` | A function of `content.addPackSource` raised an error, or its promise rejected. The load fails with `storeAssetUnavailable`. |
| `The source of packs delivered the pack "<pack>" into "<folder>", which is no folder.` | A source of packs answered with a folder that does not exist. The load fails with `storeAssetUnavailable`. |
| `The asset "<path>" is not on the device, and the app names no server to download it from.` | An asynchronous load needs an asset that is not on the device, in an app without a content server. The load fails with `storeAssetUnavailable`. |
| `The asset "<path>" of the pack "<pack>" is not on the device, no source of packs delivered the pack, and the app names no server to download it from.` | An asynchronous load needs an asset of a pack that no store installed, in an app without a content server. The load fails with `storeAssetUnavailable`. |
