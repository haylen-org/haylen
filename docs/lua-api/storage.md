# 📖 `haylen.storage`

The module `haylen.storage` reads and writes private files of the current player and app, such as caches and exported data, and keeps named save slots for app progress among them. Every app gets its own folder, named after the `identifier` in `app.json`, so two apps never see each other's files. Use it whenever the app needs to keep data between sessions. Player preferences such as volume or controls belong in [`haylen.preferences`](preferences.md) instead.

```lua
local storage = require('haylen.storage')
```

## Synchronous and asynchronous calls

The functions without a suffix read and write the disk on the frame thread and return once the work is done. That takes little time for small files on local storage, so they suit settings, small saves and loading at the start of a scene, but the time a large file or slow storage takes shows as a hitch in the frame. The functions whose names end in `Async` do the same work on the I/O pool and return a promise from Varn's `async` module at once. Call `:await()` on it inside a coroutine started with `async.spawn()` or `scene.spawn()`. It returns the result, or `nil` and the error message when the operation failed. Arguments of the wrong type and values that cannot become JSON raise at once, and every other failure, such as a path that leaves the folder, a bad slot name or a missing file, rejects the promise with the message the synchronous function raises.

Asynchronous operations run one at a time in the order they were started, so a read that starts after a write sees what the write stored. The synchronous functions do not wait for them, and a file written both ways at once ends up with one of the two contents, never with a mix. When the app stops, the operations still waiting run before it quits, so a save started just before the app closes reaches the disk.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    storage.writeJsonAsync('cache/world.json', {seed = 42, chunks = {1, 2, 3}})
    local world, err = storage.readJsonAsync('cache/world.json'):await()
    print(world and #world.chunks or err) -- 3
end)
```

## Paths

Paths are relative to the storage folder of the app and use `/` as the separator, for example `'saves/slot1.json'`. The `.` segments are ignored and `..` may only go back inside the folder. The following paths raise errors:

- An absolute path, such as `'/etc/hosts'` or `'C:/data'`, raises `The path "/etc/hosts" must be relative.`
- A path that leaves the folder, such as `'../other.txt'`, raises `The path "../other.txt" must stay inside its root folder.`
- An empty path raises `A storage path cannot be empty.`
- A path with the `.tmp` extension, such as `'cache/data.tmp'`, raises `The storage path "cache/data.tmp" ends with ".tmp", which the storage keeps for the temporary files of its writes.`

The storage folder is located here on each platform:

| Platform | Folder |
| --- | --- |
| Windows | `%APPDATA%\<identifier>` |
| macOS and iOS | The folder is `Application Support/<identifier>` in the user domain. On tvOS, it is `Caches/<identifier>`. |
| Linux | The folder is `$XDG_DATA_HOME/<identifier>`, or `~/.local/share/<identifier>` when the variable is not set. |
| Android | The folder is `<identifier>` inside the internal data folder of the app. |
| Web | The folder is `/persistent/<identifier>` in a file system that the page keeps in IndexedDB. |

## Files

### storage.readText(path)

Returns the whole content of a file as a string. Lua strings hold any bytes, so binary files work too. A missing file raises `The storage file "<path>" was not found.`

```lua
local storage = require('haylen.storage')

if storage.exists('notes.txt') then
    local notes = storage.readText('notes.txt')
    print(#notes .. ' bytes of notes')
end
```

### storage.writeText(path, text)

Writes `text` to a file, replacing any previous content and creating missing folders. The data goes to a temporary sibling file first and then replaces the target, so a crash never leaves a half-written file behind. When another program holds the file for a moment, as a virus scanner does after a file changes and which makes Windows refuse to replace it, the write tries again for a short while. A write that fails, such as over a folder or inside a path that names a file, raises `The storage file "<path>" could not be written.` and leaves the previous content and no temporary file behind.

```lua
local storage = require('haylen.storage')

storage.writeText('logs/last-run.txt', 'finished day 3 with 42 coins')
```

### storage.readJson(path)

Reads a JSON file and returns it as plain Lua values. Objects become tables with string keys, arrays become sequences and `null` becomes `nil`. A missing file raises the same error as `storage.readText`, and invalid JSON raises the parser error, which starts with `[json.exception.parse_error.101]`.

```lua
local storage = require('haylen.storage')

local progress = {day = 1, inventory = {}}
if storage.exists('saves/progress.json') then
    progress = storage.readJson('saves/progress.json')
end
print('day', progress.day)
```

### storage.writeJson(path, value)

Converts `value` to JSON and writes it with two-space indentation, with the same atomic write as `storage.writeText`. Sequences become arrays, other tables become objects, and an empty table becomes an empty object. Numeric keys of non-sequence tables become strings. Functions, userdata such as `Vec2`, threads and cyclic tables cannot be converted and raise errors such as `A function cannot be converted to JSON.` or `A userdata cannot be converted to JSON.`, and so do strings and keys that are not valid UTF-8 and numbers that are not finite, such as `0 / 0` or `math.huge`, which JSON text cannot hold.

```lua
local storage = require('haylen.storage')

local player = {x = 320, y = 180}
storage.writeJson('saves/progress.json', {
    day = 4,
    inventory = {'wood', 'stone', 'rope'},
    position = {x = player.x, y = player.y},
})
```

### storage.exists(path)

Returns `true` when `path` names an existing file. Folders return `false`.

```lua
local storage = require('haylen.storage')

local firstLaunch = not storage.exists('saves/progress.json')
print('first launch:', firstLaunch)
```

### storage.remove(path)

Deletes the file at `path`, or the folder at `path` when it is empty, and returns `true` when something was removed. A missing path or a folder that still holds files returns `false` and removes nothing.

```lua
local storage = require('haylen.storage')

local function deleteSave()
    if storage.remove('saves/progress.json') then
        print('save deleted')
    end
end

deleteSave()
```

### storage.list(directory)

Returns a sorted sequence with the paths of every file inside `directory` and its subfolders. Paths are relative to the storage folder, not to `directory`, so they can be passed straight to the other functions. Leave `directory` out to list every file of the app. A missing folder returns an empty table. Files with the `.tmp` extension are left out, because writes use them as temporary files. A folder that cannot be read raises `The storage folder "<directory>" could not be listed, and the file system reported "<reason>".`, or `The user storage could not be listed, ...` without a `directory`, instead of returning part of the files.

```lua
local storage = require('haylen.storage')

storage.writeText('saves/slot1.json', '{}')
storage.writeText('saves/slot2.json', '{}')
for _, path in ipairs(storage.list('saves')) do
    print(path) -- saves/slot1.json, then saves/slot2.json
end
print(#storage.list() .. ' files in total')
```

### storage.flush()

Makes previous writes durable on platforms that buffer storage. On the web it copies the file system to IndexedDB, and elsewhere it returns right away because writes are already on disk. Call it after saving important data, for example when a level ends. The engine flushes by itself when the app goes to the background, right after the listeners of the `appBackground` event of [`haylen.events`](events.md) have run, so saves written there are durable too.

```lua
local storage = require('haylen.storage')

local function saveAndPersist(progress)
    storage.writeJson('saves/progress.json', progress)
    storage.flush()
end

saveAndPersist({day = 5})
```

### storage.root()

Returns the absolute path of the storage folder, with `/` as the separator. Varn's `fs` module reads and writes files asynchronously on the I/O pool, and paths built from the root reach the same files as `haylen.storage`. The paths of `fs` are not confined to the folder, so build them from the root instead of passing paths from the player. The [Lua guide](../core/lua.md#disk-access) compares both ways.

```lua
local async = require('async')
local fs = require('fs')
local storage = require('haylen.storage')

async.spawn(function()
    local replays = storage.root() .. '/replays'
    fs.mkdir(replays):await()
    fs.writeFile(replays .. '/run-1.bin', string.rep('\0', 1024)):await()
    print(fs.stat(replays .. '/run-1.bin'):await().size) -- 1024
    print(storage.exists('replays/run-1.bin')) -- true
end)
```

### storage.readTextAsync(path), storage.readJsonAsync(path)

Return promises for what `storage.readText` and `storage.readJson` return, read and parsed on the I/O pool. A missing file or invalid JSON rejects the promise with the same message those functions raise.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    local notes, err = storage.readTextAsync('notes.txt'):await()
    print(notes or err)
end)
```

### storage.writeTextAsync(path, text), storage.writeJsonAsync(path, value)

Return promises that resolve with `true` once `text`, or `value` converted to JSON with two-space indentation, replaced the file with the same atomic write as `storage.writeText`. The value is converted when the function is called, so later changes to the table do not reach the file. Call `storage.flush()` after awaiting when the file must survive a crash of a web build.

```lua
local async = require('async')
local storage = require('haylen.storage')

local replay = {frames = {1, 2, 3}}
async.spawn(function()
    storage.writeJsonAsync('replays/last.json', replay):await()
    storage.flush()
end)
```

### storage.removeAsync(path), storage.listAsync(directory)

Return promises for what `storage.remove` and `storage.list` return.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    for _, path in ipairs(storage.listAsync('cache'):await()) do
        storage.removeAsync(path):await()
    end
end)
```

## Save slots

Save slots hold the progress of the app, with a small summary of each slot for a load menu. Each slot is a JSON file at `saves/<slot>.json` among the other files of the app, so `storage.list('saves')` lists them too. A slot holds the app data, a summary table for load menus, the time it was saved, the save version of the app, a revision that counts the writes of the slot and a checksum of the rest. Every write keeps the previous file of the slot as its backup at `saves/<slot>.backup.json` and flushes the new file and its folder to the disk before and after it replaces the old one, so a crash, a dead battery or a forced quit leaves the old slot or the new one, and a slot that a damaged disk or an interrupted copy broke reads its backup instead, which `slotInfo().recovered` tells. Slot names use 1 to 64 letters, digits, dashes or underscores, such as `'slot-1'` or `'auto_save'`, and other names raise `The save slot name "<slot>" must use 1 to 64 letters, digits, dashes or underscores.`. Every write and remove reaches the disk at once, including the browser storage of web builds.

Data and summaries are converted to JSON. Booleans, finite numbers, UTF-8 strings and tables of them are kept. A table whose keys are exactly 1 to its length becomes a list, and any other table becomes an object whose keys come back as strings, so `{[1] = 'a', [3] = 'c'}` reads back as `{['1'] = 'a', ['3'] = 'c'}`. An empty table reads back as an empty table. Functions, userdata and threads raise an error, and so do tables nested deeper than 128 levels, which includes tables that contain themselves, strings and keys that are not valid UTF-8 and numbers that are not finite, such as NaN or infinity.

### storage.writeSlot(slot, data, summary)

Writes `data` to the slot with the current time, replacing what the slot held. The optional `summary` is a small table shown by load menus, such as the level or the play time, and defaults to an empty table. A summary that is not a table, or that is a list, raises `A save summary must be a JSON object.`. Numeric keys of a summary that is not a list come back as strings, like those of the data.

```lua
local storage = require('haylen.storage')

local game = {day = 4, wood = 12, inventory = {'axe', 'rope'}, player = {x = 120, y = 88}}
storage.writeSlot('slot-1', game, {day = game.day, place = 'Beach'})
```

### storage.readSlot(slot)

Returns the data of the slot, or `nil` when the slot does not exist. Data of an older save version runs through the migrations of [`storage.addMigration()`](#storageaddmigrationfromversion-migration) up to the version of the app, a slot without a sound file or backup raises `The save slot "<slot>" is damaged.`, a slot of a newer version raises `The save slot "<slot>" has version <version>, which is newer than the version <version> this app reads.` and a version that no chain of migrations reaches raises `The save slot "<slot>" has version <version> and no migration reaches version <version>.`.

```lua
local storage = require('haylen.storage')

local game = storage.readSlot('slot-1')
if game then
    print(game.day, game.inventory[1], game.player.x)
end
```

### storage.slotInfo(slot)

Returns a table describing the slot, or `nil` when the slot does not exist. A damaged file raises `The save slot "<slot>" is damaged.`.

| Field | Type | Meaning |
| --- | --- | --- |
| `slot` | string | The slot name. |
| `savedAt` | integer | When the slot was written, in Unix seconds. |
| `summary` | table | The summary given to `storage.writeSlot()`. |
| `version` | integer | The save version of the app that wrote the slot. |
| `revision` | integer | How many times the slot was written, which cloud saves compare. |
| `recovered` | boolean | `true` when the slot was damaged and these fields come from its backup. |

```lua
local storage = require('haylen.storage')

local info = storage.slotInfo('slot-1')
if info then
    print(info.slot, info.savedAt, info.summary.day)
end
```

### storage.setSaveVersion(version), storage.saveVersion()

Sets the save version of the app, a whole number from 1 and 1 by default, which every write stamps and every read migrates to, and returns it. A version below 1 raises `A save version is a whole number from 1.`.

```lua
local storage = require('haylen.storage')

storage.setSaveVersion(3)
print(storage.saveVersion())
```

### storage.addMigration(fromVersion, migration)

Registers the function that turns the data of a version into the data of the next one, so an update that changes the shape of the data never strands old saves. A read of an older slot runs every step in order on the frame thread, also for `storage.readSlotAsync()`, and the stored slot keeps its version until the app writes it again. A migration takes the data and returns the new data, and it may not yield. A version below 1 raises `A save migration needs a version from 1 and a function.`.

```lua
local storage = require('haylen.storage')

storage.setSaveVersion(3)
storage.addMigration(1, function(data)
    data.inventory = data.items
    data.items = nil
    return data
end)
storage.addMigration(2, function(data)
    data.player.stamina = 100
    return data
end)
local game = storage.readSlot('slot-1')
```

### storage.exportSlot(slot), storage.importSlot(slot, bytes, options)

The function `exportSlot` returns the file of a slot as a string of bytes, or `nil`, which a cloud save plugin uploads, and `importSlot` writes such bytes as a slot and returns whether it did. The option `policy` is `'keepNewer'` by default, which imports only bytes of a higher revision, or of the same revision and a later save time, than the slot of the device, so progress made on another device never disappears under an older copy, or `'replace'`, which imports them whatever the device holds. Bytes that are not a whole slot raise `The imported save slot "<slot>" is damaged.` and another policy raises `The policy of an imported save slot must be "replace" or "keepNewer".`.

```lua
local events = require('haylen.events')
local platform = require('haylen.platform')
local storage = require('haylen.storage')

events.on('saveSlotWritten', function(info)
    platform.plugin('cloud-saves'):call('upload', {slot = info.slot, data = platform.bytes(storage.exportSlot(info.slot))})
end)

local function downloaded(slot, bytes)
    if storage.importSlot(slot, bytes, {policy = 'keepNewer'}) then
        print('The cloud held newer progress.')
    end
end
```

### storage.slotExists(slot)

Returns `true` when the slot exists.

```lua
local storage = require('haylen.storage')

local label = storage.slotExists('auto_save') and 'Continue' or 'New game'
print(label)
```

### storage.removeSlot(slot)

Deletes the slot with its backup and returns `true`, or returns `false` when the slot did not exist.

```lua
local storage = require('haylen.storage')

if storage.removeSlot('slot-2') then
    print('slot 2 deleted')
end
```

### storage.listSlots()

Returns a list of `storage.slotInfo()` tables for every slot, newest first, with slots saved in the same second ordered by name. Files in the `saves` folder that are not slots are skipped, and a damaged slot raises `The save slot "<slot>" is damaged.`.

```lua
local storage = require('haylen.storage')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.slots = storage.listSlots()
    end,
    render = function(self)
        graphics2d.beginScreen()
        for index, info in ipairs(self.slots) do
            graphics2d.drawText(nil, info.slot .. '  day ' .. tostring(info.summary.day), 80, 60 + index * 40)
        end
    end,
})
```

### storage.writeSlotAsync(slot, data, summary), storage.removeSlotAsync(slot)

Return promises for what `storage.writeSlot` and `storage.removeSlot` do. The data and summary are converted when the function is called, the file is written or removed on the I/O pool, and the promise settles once the change is durable, including the browser storage of web builds. The function `storage.writeSlotAsync` resolves with `true`, and `storage.removeSlotAsync` with whether the slot existed. A value that cannot become JSON raises at once, and a bad slot name or a summary that is not a table, or that is a list, rejects the promise.

```lua
local async = require('async')
local storage = require('haylen.storage')

local game = {day = 7, wood = 30}
async.spawn(function()
    local saved, err = storage.writeSlotAsync('auto_save', game, {day = game.day}):await()
    print(saved and 'saved' or err)
end)
```

### storage.readSlotAsync(slot), storage.slotInfoAsync(slot), storage.listSlotsAsync()

Return promises for what `storage.readSlot`, `storage.slotInfo` and `storage.listSlots` return. A damaged slot rejects the promise with `The save slot "<slot>" is damaged.`.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    for _, info in ipairs(storage.listSlotsAsync():await()) do
        local game = storage.readSlotAsync(info.slot):await()
        print(info.slot, info.savedAt, game and game.day)
    end
end)
```

### Autosave example

```lua
local storage = require('haylen.storage')
local timer = require('haylen.timer')
local scene = require('haylen.scene')

local game = storage.readSlot('auto_save') or {day = 1, wood = 0}

scene.push({
    enter = function(self)
        self.autosave = timer.every(60, function()
            storage.writeSlotAsync('auto_save', game, {day = game.day})
        end)
    end,
    exit = function(self)
        timer.cancel(self.autosave)
        storage.writeSlot('auto_save', game, {day = game.day})
    end,
})
```

## Resume state

Mobile systems end apps in the background to free memory, and the player who comes back expects the app where they left it, not at the start. The resume state is a value that only that launch reads: the app keeps its current state with `storage.setResumeState`, the engine writes it to the storage folder when the app goes to the background, after the listeners of `appBackground` ran, and removes it when the app comes back to the foreground or stops normally. A launch after the system ended the app finds it with `storage.resumeState`, and a launch after the player quit finds nothing. On the web, a page that the browser closes or discards in the background leaves the state as well.

### storage.setResumeState(value)

Keeps `value`, any value that can become JSON, for the next time the app goes to the background, or nothing for `nil`. Call it whenever what the player would want back changes, such as the screen, the open dialog or the scroll of a list, which costs nothing until the app leaves the foreground. A value that cannot become JSON raises the errors of the [save slots](#save-slot-errors).

### storage.resumeState()

Returns the value that the run before kept and how many seconds ago that run went to the background, or nothing. The engine reads it once when the app starts and removes it from the disk, so a later launch never finds it again, and an app decides by the age whether the state is still worth restoring.

```lua
local scene = require('haylen.scene')
local storage = require('haylen.storage')

local Shop = require('scenes.shop')
local Menu = require('scenes.menu')

local state, age = storage.resumeState()
if state and state.screen == 'shop' and age < 3600 then
    scene.push(Shop(state.tab))
else
    scene.push(Menu())
end
storage.setResumeState({screen = 'menu'})
```

## Save slot errors

| Message | Cause |
| --- | --- |
| `The save slot name "<slot>" must use 1 to 64 letters, digits, dashes or underscores.` | The slot name is empty, too long or has other characters. |
| `A save summary must be a JSON object.` | The summary is not a table, or it is a list. |
| `The save slot "<slot>" is damaged.` | The slot file and its backup are not valid JSON, miss their data, summary, time, version, revision or checksum, or their checksum does not match. |
| `The save slot "<slot>" has version <version>, which is newer than the version <version> this app reads.` | A slot written by a newer version of the app. |
| `The save slot "<slot>" has version <version> and no migration reaches version <version>.` | A step of the migrations is missing. |
| `A save version is a whole number from 1.` | The function `storage.setSaveVersion()` received a version below 1. |
| `A save migration needs a version from 1 and a function.` | The function `storage.addMigration()` received a version below 1. |
| `The imported save slot "<slot>" is damaged.` | The function `storage.importSlot()` received bytes that are not a whole slot. |
| `The policy of an imported save slot must be "replace" or "keepNewer".` | The function `storage.importSlot()` received another policy. |
| `A <type> cannot be converted to JSON.` | The data or summary holds a function, userdata or thread. |
| `A string that is not valid UTF-8 cannot be converted to JSON.` | The data or summary holds a string that is not valid UTF-8. |
| `A key that is not valid UTF-8 cannot be converted to JSON.` | The data or summary holds a table with a key that is not valid UTF-8. |
| `A number that is not finite cannot be converted to JSON.` | The data or summary holds NaN or an infinity. |
| `Value is nested too deeply to convert to JSON.` | The data or summary nests tables deeper than 128 levels or contains itself. |
