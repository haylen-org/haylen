# haylen.content

The module `haylen.content` tells which content the app plays and which of its assets are on the device, brings assets down ahead of time without loading them, and checks, downloads and applies the updates of the content. Most apps never need it: assets load with [`haylen.assets`](assets.md), which brings assets that are not on the device before it reads them, and the engine downloads the content an app needs to start, the prefetched content and its updates by itself. The module serves apps that show the state of their content, bring a region of their world down while the player is elsewhere, or decide themselves when to update.

```lua
local content = require('haylen.content')
```

The [content guide](../content/content.md) describes how content reaches the device: `required` assets are on the device before the app starts, `prefetch` assets download in the background once it runs, and `onDemand` assets download only when the app asks for them, as the `content` section of `app.json` decides. The [loading guide](../content/loading.md) shows with complete examples how deliveries, preload groups, scene loads and the packages of each store work together.

## Development and releases

In development the app plays its folder. There are no downloads and no updates, but the delivery of `app.json` still applies: an asset that downloads later in a release is not on the device until something brings it, so `assets.load` of an `onDemand` asset fails in development exactly as it would in a release, and `assets.loadAsync`, a preload group or `content.prefetch` bring it in the next frame. A release build plays its protected release, and with a content server in `content.updates` of `app.json` it downloads what is not on the device and checks its channel for updates.

## State

### content.isProtected()

Returns `true` when the app plays a protected release, as a release build does, and `false` in development.

### content.generation()

Returns the generation of the content that plays: `1` for the content that ships with a release, the generation of its channel for an update that the app installed, and `0` in development.

### content.channel()

Returns the update channel that `content.updates` of `app.json` names, such as `'stable'`, or `nil` for an app without a content server.

### content.status()

Returns what the content does now: `'idle'`, `'checking'` while it asks the channel for an update, `'downloading'` while it brings the content the app needs to start or an update, `'ready'` once an update is installed and plays from the next start, and `'failed'` after an error, which `content.error()` describes.

### content.progress()

Returns the progress of the download of the required content or of an update as a table with `receivedBytes` and `totalBytes`.

### content.error()

Returns the last failure as a table, or `nil`:

| Field | Meaning |
| --- | --- |
| `code` | The kind of failure, such as `'networkOffline'`, `'networkTimeout'`, `'insufficientStorage'`, `'quotaExceeded'`, `'rangeUnsupported'`, `'manifestIncompatible'` or `'manifestRollbackRejected'`, as the [content guide](../content/content.md#errors) lists them. |
| `message` | The sentence that explains it. |
| `retryable` | Whether trying again may succeed without a new build of the app, such as once the network comes back or room is freed. |
| `httpStatus` | The status the server answered, when the failure was one. |
| `requiredBytes`, `availableBytes` | The room an update needs and the room that is free, when the device lacked it. |
| `path` | The file of the package that the app read, such as `content/levels/forest.json`, when a read failed. |
| `manifestId`, `packId`, `contentId`, `storedId`, `keyId` | The hexadecimal IDs of the manifest, the pack, the plain content, the stored record and the content key involved, when the failure knows them, such as the key of an update that no key provider of the app gives. |
| `platform` | The content profile of the app, when a manifest was made for another one. |
| `platformMessage` | What the system or the browser said, such as the reason of a refused write. |

The promises of `content.prefetch`, `content.check` and `content.download` reject with the same table: their `await` gives `nil`, the message and the table, which also holds the message in `error`, and the second argument of a `catch` handler is the table.

```lua
local content = require('haylen.content')

local failure = content.error()
if failure and failure.code == 'insufficientStorage' then
    print('Free ' .. (failure.requiredBytes - failure.availableBytes) .. ' bytes to update.')
end

require('async').spawn(function()
    local done, message, prefetchFailure = content.prefetch({'levels/forest.json'}):await()
    if not done and prefetchFailure.retryable then
        print('The level downloads later: ' .. message)
    end
end)
```

### content.update()

Returns the update that the last check found, as a table with `generation`, `downloadBytes`, the bytes that installing it downloads, and `reusedBytes`, the bytes of its required content that the device already holds and that installing it never downloads again, or `nil`.

### content.isLocal(path)

Returns whether every byte of an asset, by its path relative to the `content` folder, is on the device, so loading it never waits for a download. An asset that does not exist counts as on the device, and loading it reports that it is missing. On the web, where the content store lives in the browser, an asset counts once the page read its bytes in during the visit, which `assets.loadAsync`, a preload group or `content.prefetch` does without a download when the store holds them, while the required content is always read in before the app starts.

```lua
local content = require('haylen.content')

if not content.isLocal('world/asia/maps/tokyo.tmj') then
    print('Tokyo downloads when the player goes there.')
end
```

### content.isGroupLocal(name)

Returns whether every asset of the [preload group](assets.md#preload-groups) `name` is on the device, so preloading it reads the bytes from the device at once and never waits for a download. A group that is not defined raises `The asset group "<name>" is not defined.`

```lua
local assets = require('haylen.assets')
local content = require('haylen.content')

assets.defineGroups('groups.json')
if not content.isGroupLocal('asia') then
    content.prefetch('asia')
end
```

### content.addPackFolder(path)

Reads the shards of a pack from a folder where a store installed them while the app runs, so the assets of the pack load from then on, without a restart. A native plugin of a store that delivers packs on request, such as an asset pack that a store downloads when the player buys an expansion, calls it with the folder the store reports. Every shard is checked against the signed manifest of the release, so a folder never brings content that the release does not name. In development the folder of the app holds every pack, so the call does nothing. A path that is no folder raises `The pack folder "<path>" does not exist.`

```lua
local content = require('haylen.content')

-- The plugin of the store answers with the folder of the pack it installed.
content.addPackFolder(folder)
print(content.isLocal('expansion/island.json'))
```

### content.addPackSource(fn, options)

Adds a source of packs and returns a [`Connection`](signal.md#connection) that removes it. When the app loads an asset of a pack that the device lacks, with `assets.loadAsync`, `assets.preload` or `content.prefetch`, the engine asks each source in the order they were added for the pack, before the content server, so the code that loads the asset never knows which store delivers it. The function `fn` runs as a task with the name of the pack and a progress function, which takes a table with `receivedBytes` and `totalBytes`, and returns the folder that holds the shards of the delivered pack, `nil` when it does not deliver the pack, which passes it on to the next source, or a promise of either. The folder joins the release as `content.addPackFolder` adds it. An error of `fn` or a promise that rejects fails the load with `storeAssetUnavailable` and the message `The source of packs failed to deliver the pack "<pack>". <reason>`, a folder that does not exist with `The source of packs delivered the pack "<pack>" into "<folder>", which is no folder.`, and any other value with `The source of packs must return the folder of the pack, nil or a promise of either.` A load whose pack no source delivers comes from the content server, or fails with `storeAssetUnavailable` in an app without one. The options table takes `owner`, whose end removes the source and stops its tasks, and any other key raises `Unknown option "<key>".` A source that is removed while it delivers counts as one that delivers nothing. In development the folder of the app holds every pack, so no source is asked.

```lua
local content = require('haylen.content')
local store = require('store-packs')

-- The Lua part of a store plugin hands every pack request to its native part, which answers with the folder where the store installed the pack.
content.addPackSource(function(pack, progress)
    return store.download(pack, progress)
end, {owner = self})
```

### content.stats()

Returns counters of the content for debugging, which the Content section of the [debug overlay](debug.md) shows too: `generation`, `catalogFiles`, the files the content holds, `storedRecords` and `storedBytes`, what the local store of downloads holds, `openShards`, the shard files open now, `decodedChunks` and `decodedBytes`, the chunks read since the content was mounted and their plain bytes, `decodeMilliseconds`, the time that reading, decrypting, decompressing and checking them took on every thread, `activeReads`, the chunk reads that run now, `downloadedBytes` and `requests` since the app started, `resumedRecords`, the downloads that went on from where an earlier one stopped, `pendingDownloads`, which is `1` while anything downloads, `wantedRecords`, the records that downloads still wait for, `activeTransfers`, the range requests that run now, `downloadRate`, the bytes per second over the last half second, `reusedBytes`, the bytes that the offered update reuses, and what a survey of the content on a worker counts, at most once a second while something reads the statistics, so the first call returns them as zero: `installedShards`, the shards that the device holds as files, installed with the app or by a store, `remoteShards`, the shards whose records the local store or the server brings instead, `missingChunks`, the chunks of the content that the device holds in neither, and `pendingPrefetch`, the prefetched chunks among them.

## Bringing assets

### content.prefetch(groupOrPaths, progress, options)

Brings assets onto the device without loading them, and returns a promise that resolves to `true` once every one of them is there. The argument `groupOrPaths` is the name of a [preload group](assets.md#preload-groups), whose folders count with every asset in them, or a list of asset paths. The optional `progress` function receives a table with `receivedBytes` and `totalBytes` as bytes arrive. The options table takes `owner`, which ends the progress function with its owner, and any other key raises `Unknown option "<key>".`

The promise rejects with the failure, its message and the table of [`content.error`](#contenterror), such as a device that is offline or one without room for the assets and the margin the store keeps free, and the rejection never stops the app when nothing awaits it. The assets download as foreground work, ahead of the prefetched content of `app.json`. An asset that does not exist raises `The asset "<path>" does not exist.`, and a group that is not defined raises `The asset group "<name>" is not defined.`

A later `assets.load`, `assets.loadAsync` or preload of the group reads the bytes from the device at once, and only then decodes the assets into memory, so prefetching costs storage and no memory.

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

## Updates

An app with a content server and `enabled` updates checks its channel when it starts, downloads newer content in the background and plays it from its next start, so most apps never call these functions. An app whose `content.updates` sets `enabled` to `false` decides itself when to check and download.

### content.check()

Asks the channel for newer content and returns a promise that resolves to a table with `generation` and `downloadBytes`, or to `nil` when the app plays the newest content. The promise rejects with the failure, its message and the table of [`content.error`](#contenterror), such as `manifestRollbackRejected` for a server that offers an older generation or `manifestIncompatible` for content that needs a newer build of the app. In development it raises `The app plays its folder in development, which has no updates. Build it in the Release configuration to check for updates.`, and in a release without a content server `The app names no content server, so it has no updates. Name one in "content.updates" of "app.json".`

### content.download(progress, options)

Installs the update that the last check found, checking first when none was found, and returns a promise that resolves to `true` once the update is on the device and plays from the next start. The device must have room for the download and a margin first, or the promise rejects with `insufficientStorage`. The `progress` function and the options work as in `content.prefetch`. A download that stops resumes where it stopped the next time, and the content that plays never changes until the whole update is verified.

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

Stops the check or the download that runs, whose promise rejects with `networkCancelled`. The bytes that arrived stay for the next attempt.

## Events

The content publishes these events on [`haylen.events`](events.md):

| Event | Data |
| --- | --- |
| `contentUpdateAvailable` | `generation` and `downloadBytes` of the update that a check found. |
| `contentUpdateReady` | `generation` of the update that is installed and plays from the next start. |
| `contentUpdateFailed` | `code` and `message` of a check or a download that failed. |
| `contentRecovered` | `manifest`, the ID of downloaded content that could not play again, and `reason`, once the app fell back to earlier content when it started. |

```lua
local events = require('haylen.events')
local content = require('haylen.content')

events.on('contentUpdateReady', function()
    print('The new content plays from the next start, or now with content.apply().')
end)
```
