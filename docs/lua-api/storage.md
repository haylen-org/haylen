# haylen.storage

`haylen.storage` reads and writes private files of the current player and app, such as caches and exported data, and keeps named save slots for app progress among them. Every app gets its own folder, named after the `identifier` in `app.json`, so two apps never see each other's files. Use it whenever the app needs to keep data between sessions. Player preferences such as volume or controls belong in [haylen.preferences](preferences.md) instead.

```lua
local storage = require('haylen.storage')
```

## Paths

Paths are relative to the storage folder of the app and use `/` as the separator, for example `'saves/slot1.json'`. `.` segments are ignored and `..` may only go back inside the folder. The following paths raise errors:

- An absolute path, such as `'/etc/hosts'` or `'C:/data'`, raises `Paths must be relative: /etc/hosts`.
- A path that leaves the folder, such as `'../other.txt'`, raises `Paths cannot leave their root folder: ../other.txt`.
- An empty path raises `A storage path cannot be empty.`

The storage folder is located here on each platform:

| Platform | Folder |
| --- | --- |
| Windows | `%APPDATA%\<identifier>` |
| macOS and iOS | `Application Support/<identifier>` in the user domain. tvOS uses `Caches/<identifier>`. |
| Linux | `$XDG_DATA_HOME/<identifier>`, or `~/.local/share/<identifier>` when the variable is not set. |
| Android | `<identifier>` inside the internal data folder of the app. |
| Web | `/persistent/<identifier>` in a file system that the page keeps in IndexedDB. |

## Files

### storage.read(path)

Returns the whole content of a file as a string. Lua strings hold any bytes, so binary files work too. A missing file raises `Storage file was not found: <path>`.

```lua
local storage = require('haylen.storage')

if storage.exists('notes.txt') then
    local notes = storage.read('notes.txt')
    print(#notes .. ' bytes of notes')
end
```

### storage.write(path, text)

Writes `text` to a file, replacing any previous content and creating missing folders. The data goes to a temporary sibling file first and then replaces the target, so a crash never leaves a half-written file behind.

```lua
local storage = require('haylen.storage')

storage.write('logs/last-run.txt', 'finished day 3 with 42 coins')
```

### storage.readJson(path)

Reads a JSON file and returns it as plain Lua values. Objects become tables with string keys, arrays become sequences and `null` becomes `nil`. A missing file raises the same error as `storage.read`, and invalid JSON raises the parser error, which starts with `[json.exception.parse_error.101]`.

```lua
local storage = require('haylen.storage')

local progress = {day = 1, inventory = {}}
if storage.exists('saves/progress.json') then
    progress = storage.readJson('saves/progress.json')
end
print('day', progress.day)
```

### storage.writeJson(path, value)

Converts `value` to JSON and writes it with two-space indentation, with the same atomic write as `storage.write`. Sequences become arrays, other tables become objects, and an empty table becomes an empty object. Numeric keys of non-sequence tables become strings. Functions, userdata such as `Vec2`, threads and cyclic tables cannot be converted and raise errors such as `A function cannot be converted to JSON.` or `A userdata cannot be converted to JSON.`

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

Returns a sorted sequence with the paths of every file inside `directory` and its subfolders. Paths are relative to the storage folder, not to `directory`, so they can be passed straight to the other functions. Leave `directory` out to list every file of the app. A missing folder returns an empty table. Files with the `.tmp` extension are left out, because writes use them as temporary files.

```lua
local storage = require('haylen.storage')

storage.write('saves/slot1.json', '{}')
storage.write('saves/slot2.json', '{}')
for _, path in ipairs(storage.list('saves')) do
    print(path) -- saves/slot1.json, then saves/slot2.json
end
print(#storage.list() .. ' files in total')
```

### storage.flush()

Makes previous writes durable on platforms that buffer storage. On the web it copies the file system to IndexedDB, and elsewhere it returns right away because writes are already on disk. Call it after saving important data, for example when a level ends. The engine flushes by itself when the app goes to the background, right after the listeners of the `app_background` event of [haylen.events](events.md) have run, so saves written there are durable too.

```lua
local storage = require('haylen.storage')

local function saveAndPersist(progress)
    storage.writeJson('saves/progress.json', progress)
    storage.flush()
end

saveAndPersist({day = 5})
```

### storage.root()

Returns the absolute path of the storage folder, with `/` as the separator. Varn's `fs` module reads and writes files asynchronously on the I/O pool, and paths built from the root reach the same files as `haylen.storage`. The paths of `fs` are not confined to the folder, so build them from the root instead of passing paths from the player. The [Lua guide](../lua.md#disk-access) compares both ways.

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

## Save slots

Save slots hold the progress of the app, with a small summary of each slot for a load menu. Each slot is a JSON file at `saves/<slot>.json` among the other files of the app, so `storage.list('saves')` lists them too. A slot holds the app data, a summary table for load menus and the time it was saved. Slot names use 1 to 64 letters, digits, dashes or underscores, such as `'slot-1'` or `'auto_save'`, and other names raise `A save slot name uses 1 to 64 letters, digits, dashes or underscores: <slot>`. Every write and remove reaches the disk at once, including the browser storage of web builds.

Data and summaries are converted to JSON. Booleans, numbers, strings and tables of them are kept. A table whose keys are exactly 1 to its length becomes a list, and any other table becomes an object whose keys come back as strings, so `{[1] = 'a', [3] = 'c'}` reads back as `{['1'] = 'a', ['3'] = 'c'}`. An empty table reads back as an empty table. Functions, userdata and threads raise an error, and so do tables nested deeper than 128 levels, which includes tables that contain themselves.

### storage.writeSlot(slot, data, summary)

Writes `data` to the slot with the current time, replacing what the slot held. The optional `summary` is a small table shown by load menus, such as the level or the play time, and defaults to an empty table. A summary that is not a table with string keys raises `A save summary must be a JSON object.`.

```lua
local storage = require('haylen.storage')

local game = {day = 4, wood = 12, inventory = {'axe', 'rope'}, player = {x = 120, y = 88}}
storage.writeSlot('slot-1', game, {day = game.day, place = 'Beach'})
```

### storage.readSlot(slot)

Returns the data of the slot, or `nil` when the slot does not exist. A file that is not a valid save raises `The save slot <slot> is damaged.`.

```lua
local storage = require('haylen.storage')

local game = storage.readSlot('slot-1')
if game then
    print(game.day, game.inventory[1], game.player.x)
end
```

### storage.slotInfo(slot)

Returns a table describing the slot, or `nil` when the slot does not exist. A damaged file raises `The save slot <slot> is damaged.`.

| Field | Type | Meaning |
| --- | --- | --- |
| `slot` | string | The slot name. |
| `savedAt` | integer | When the slot was written, in Unix seconds. |
| `summary` | table | The summary given to `storage.writeSlot()`. |

```lua
local storage = require('haylen.storage')

local info = storage.slotInfo('slot-1')
if info then
    print(info.slot, info.savedAt, info.summary.day)
end
```

### storage.slotExists(slot)

Returns true when the slot exists.

```lua
local storage = require('haylen.storage')

local label = storage.slotExists('auto_save') and 'Continue' or 'New game'
print(label)
```

### storage.removeSlot(slot)

Deletes the slot and returns true, or returns false when the slot did not exist.

```lua
local storage = require('haylen.storage')

if storage.removeSlot('slot-2') then
    print('slot 2 deleted')
end
```

### storage.listSlots()

Returns a list of `storage.slotInfo()` tables for every slot, newest first, with slots saved in the same second ordered by name. Files in the `saves` folder that are not slots are skipped, and a damaged slot raises `The save slot <slot> is damaged.`.

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

### Autosave example

```lua
local storage = require('haylen.storage')
local timer = require('haylen.timer')
local scene = require('haylen.scene')

local game = storage.readSlot('auto_save') or {day = 1, wood = 0}

scene.push({
    enter = function(self)
        self.autosave = timer.every(60, function()
            storage.writeSlot('auto_save', game, {day = game.day})
        end)
    end,
    exit = function(self)
        timer.cancel(self.autosave)
        storage.writeSlot('auto_save', game, {day = game.day})
    end,
})
```

## Save slot errors

| Message | Cause |
| --- | --- |
| `A save slot name uses 1 to 64 letters, digits, dashes or underscores: <slot>` | The slot name is empty, too long or has other characters. |
| `A save summary must be a JSON object.` | The summary is not a table with string keys. |
| `The save slot <slot> is damaged.` | The slot file is not valid JSON, misses its data, summary or time, or holds a summary that is not an object or a time that is not an integer. |
| `A <type> cannot be converted to JSON.` | The data or summary holds a function, userdata or thread. |
| `Value is nested too deeply to convert to JSON.` | The data or summary nests tables deeper than 128 levels or contains itself. |
