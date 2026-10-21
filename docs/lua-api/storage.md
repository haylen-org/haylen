# 📖 `haylen.storage`

The module `haylen.storage` reads and writes the private files of the player, such as caches, exported data and logs. It also keeps named save slots for the progress of the app. Its resume state brings the player back where they were after the system ended the app. Use it whenever the app keeps data between sessions. Choices of the player, such as the volume or the controls, belong in [`haylen.preferences`](preferences.md) instead. The [Lua guide](../core/lua.md#disk-access) explains when to use this module and when the `fs` module of Varn.

```lua
local storage = require('haylen.storage')
```

## The storage folder

Every app gets its own folder, named after the `identifier` in `app.json`, so two apps never see each other's files.

| Platform | Folder |
| --- | --- |
| Windows | `%APPDATA%\<identifier>`, in the roaming application data of the user. |
| macOS, iOS, iPadOS and Mac Catalyst | `Application Support/<identifier>` in the user domain. |
| tvOS | `Caches/<identifier>`. tvOS gives apps no permanent local folder, so the system may clear this one. |
| Linux | `$XDG_DATA_HOME/<identifier>`, or `~/.local/share/<identifier>` when the variable is not set. |
| Android | `<identifier>` inside the internal data folder of the app. |
| Web | `/persistent/<identifier>`, in a file system that the page keeps in IndexedDB. |

### Paths

Every path is relative to the storage folder, such as `'data/progress.json'`. Slashes and backslashes both separate folders, `.` segments are ignored and `..` may only go back inside the folder.

- An absolute path, such as `'/etc/hosts'` or `'C:/data'`, raises `The path "<path>" must be relative.`.
- A path that leaves the folder, such as `'../other.txt'`, raises `The path "<path>" must stay inside its root folder.`.
- An empty path raises `A storage path cannot be empty.`.
- A path with the `.tmp` extension raises `The storage path "<path>" ends with ".tmp", which the storage keeps for the temporary files of its writes.`.

Every write goes to a temporary sibling file with the `.tmp` extension first. The storage removes the temporary files that a crash left behind when the app starts.

## JSON values

Some functions take a Lua value and keep it as JSON: `storage.writeJson`, the save slots, the resume state and every value of [`haylen.preferences`](preferences.md). They all convert it the same way, and they raise at once for a value that JSON cannot hold. So a bad value never breaks a later save.

- Booleans, numbers and strings stay as they are, and `nil` becomes `null`.
- A table whose keys are exactly the integers from 1 to its length becomes a list.
- Any other table becomes an object. Its number keys become strings, so `{[1] = 'a', [3] = 'c'}` reads back as `{['1'] = 'a', ['3'] = 'c'}`.
- An empty table becomes an empty object, and it reads back as an empty table.
- Tables are read without their metatables, so an instance of a class keeps only its own fields.

These values raise an error:

- Functions, threads and userdata, such as a `Vec2`, raise `A <type> cannot be converted to JSON.`.
- A key that is not a string or a number raises `A <type> key cannot be converted to JSON.`.
- Tables nested deeper than 128 levels raise `Value is nested too deeply to convert to JSON.`. A table that contains itself raises it too.
- A string that is not valid UTF-8 raises `A string that is not valid UTF-8 cannot be converted to JSON.`.
- A key that is not valid UTF-8 raises `A key that is not valid UTF-8 cannot be converted to JSON.`.
- A number that is not finite, such as `0 / 0` or `math.huge`, raises `A number that is not finite cannot be converted to JSON.`.

JSON read back becomes plain Lua values. Objects become tables with string keys, arrays become sequences and `null` becomes `nil`. A document nested deeper than 128 levels raises `JSON value is nested too deeply to push to Lua.`.

## Calls on the I/O pool

The functions without a suffix work on the frame thread. They return once the disk work is done. Small files on local storage take little time, so these functions suit settings, small saves and loads at the start of a scene. A large file or slow storage holds up the frame, and the player sees a hitch.

The functions whose names end in `Async` do the same work on the I/O pool. They return at once with a promise of the `async` module of Varn. Call `:await()` on it inside a task, such as one that `async.spawn` or `scene.spawn` starts. It returns the result, or `nil` and the error message. The [Lua guide](../core/lua.md#asynchronous-code) explains tasks and promises.

- Arguments of the wrong type and values that cannot become JSON raise at once.
- Every other failure rejects the promise with the message that the function without `Async` raises. That covers a path that leaves the folder, a bad slot name and a missing file.
- A promise that rejects while nothing awaits it shows the error screen.

The asynchronous calls run one at a time, in the order they started. So a read that starts after a write sees what the write stored. The functions without a suffix do not wait for them. A file that both kinds write at the same time ends up with one of the two contents, never a mix. When the app stops, the calls still waiting run before it quits, so a save started just before the app closes reaches the disk.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    storage.writeJsonAsync('cache/world.json', {seed = 42, chunks = {1, 2, 3}})
    local world, failure = storage.readJsonAsync('cache/world.json'):await()
    print(world and #world.chunks or failure) -- 3
end)
```

## Files

### storage.readText(path)

Returns the whole content of a file as a string. Lua strings hold any bytes, so binary files work too. A missing file raises `The storage file "<path>" was not found.`, and a file that cannot be read raises `The storage file "<path>" could not be read.`.

```lua
local storage = require('haylen.storage')

if storage.exists('notes.txt') then
    local notes = storage.readText('notes.txt')
    print('The notes take ' .. #notes .. ' bytes.')
end
```

### storage.writeText(path, text)

Writes `text` to a file. It replaces any previous content and creates the missing folders. The text goes to a temporary sibling file first, which then replaces the file. So a crash of the app never leaves a half-written file.

Another program may hold the file for a moment, such as a virus scanner after a file changes, and Windows then refuses to replace it. The write tries again for a short while in that case. A write that still fails raises `The storage file "<path>" could not be written.`, such as a write over a folder or below a path that names a file. It leaves the previous content and no temporary file behind.

```lua
local storage = require('haylen.storage')

storage.writeText('logs/last-run.txt', 'Finished day 3 with 42 coins.')
```

### storage.readJson(path)

Reads a JSON file and returns it as plain Lua values, as [JSON values](#json-values) describes. A missing file raises the same error as `storage.readText`. Invalid JSON raises the message of the parser, which starts with `[json.exception.parse_error.101]`.

```lua
local storage = require('haylen.storage')

local progress = {day = 1, inventory = {}}
if storage.exists('data/progress.json') then
    progress = storage.readJson('data/progress.json')
end
print('Day ' .. progress.day .. '.')
```

### storage.writeJson(path, value)

Converts `value` to JSON, as [JSON values](#json-values) describes, and writes it with an indentation of two spaces. It writes the file the same safe way as `storage.writeText`.

```lua
local storage = require('haylen.storage')

local player = {x = 320, y = 180}
storage.writeJson('data/progress.json', {
    day = 4,
    inventory = {'wood', 'stone', 'rope'},
    position = {x = player.x, y = player.y},
})
```

### storage.exists(path)

Returns `true` when `path` names a file. A folder returns `false`.

```lua
local storage = require('haylen.storage')

if not storage.exists('data/progress.json') then
    print('This is the first launch.')
end
```

### storage.remove(path)

Deletes the file at `path`, or the folder at `path` when it is empty. Returns `true` when it removed something. A missing path or a folder that still holds files returns `false` and removes nothing.

```lua
local storage = require('haylen.storage')

if storage.remove('data/progress.json') then
    print('The progress was deleted.')
end
```

### storage.list(directory)

Returns a sorted list with the path of every file inside `directory` and its subfolders. The paths are relative to the storage folder, not to `directory`, so they go straight to the other functions. Leave out `directory` to list every file of the app.

- A missing folder returns an empty table.
- Files with the `.tmp` extension are left out, because writes use them as temporary files.
- A folder that cannot be read raises `The storage folder "<directory>" could not be listed, and the file system reported "<reason>".`. It never returns part of the files.
- Without `directory`, that error is `The user storage could not be listed, and the file system reported "<reason>".`.

```lua
local storage = require('haylen.storage')

storage.writeText('replays/run-1.txt', 'Jump, jump, dash.')
storage.writeText('replays/run-2.txt', 'Dash, jump.')
for _, path in ipairs(storage.list('replays')) do
    print(path) -- replays/run-1.txt, then replays/run-2.txt
end
print(#storage.list() .. ' files in total.')
```

### storage.flush()

Makes the writes so far durable on platforms that buffer storage. On the web it copies the file system of the page to IndexedDB. Elsewhere the writes are already on the disk, so it returns at once. Call it after you save important data, such as at the end of a level.

The engine also flushes by itself when the app goes to the background. It does so after the listeners of the `appBackground` event of [`haylen.events`](events.md) have run, so what they save is durable too. The [lifecycle guide](../core/lifecycle.md#app-states) explains the background.

```lua
local storage = require('haylen.storage')

local function finishLevel(progress)
    storage.writeJson('data/progress.json', progress)
    storage.flush()
end

finishLevel({day = 5})
```

### storage.root()

Returns the absolute path of the storage folder, with `/` as the separator. The `fs` module of Varn reads and writes files on the I/O pool, and paths built from the root reach the same files as `haylen.storage`. The paths of `fs` are not kept inside the folder. So build them from the root, and never pass a path that comes from the player.

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

## Files on the I/O pool

These functions do what the functions of [Files](#files) do, on the I/O pool, as [calls on the I/O pool](#calls-on-the-io-pool) describes.

### storage.readTextAsync(path)

Returns a promise of what `storage.readText` returns. A missing file rejects it with the same message.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    local notes, failure = storage.readTextAsync('notes.txt'):await()
    print(notes or failure)
end)
```

### storage.readJsonAsync(path)

Returns a promise of what `storage.readJson` returns. The file is read and parsed on the I/O pool. A missing file or invalid JSON rejects the promise with the same message that `storage.readJson` raises.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    local world, failure = storage.readJsonAsync('cache/world.json'):await()
    print(world and world.seed or failure)
end)
```

### storage.writeTextAsync(path, text)

Returns a promise that resolves with `true` once `text` replaced the file, with the same safe write as `storage.writeText`.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    local written, failure = storage.writeTextAsync('logs/session.txt', 'The session started.'):await()
    print(written and 'The log was written.' or failure)
end)
```

### storage.writeJsonAsync(path, value)

Returns a promise that resolves with `true` once `value`, converted to JSON with an indentation of two spaces, replaced the file. The value is converted when you call the function, so later changes to the table do not reach the file. Call `storage.flush()` after you await it when the file must survive a crash of a web build.

```lua
local async = require('async')
local storage = require('haylen.storage')

local replay = {frames = {1, 2, 3}}
async.spawn(function()
    storage.writeJsonAsync('replays/last.json', replay):await()
    storage.flush()
end)
```

### storage.removeAsync(path)

Returns a promise of what `storage.remove` returns: `true` when it removed something.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    local removed = storage.removeAsync('cache/world.json'):await()
    print(removed and 'The cache was cleared.' or 'There was no cache.')
end)
```

### storage.listAsync(directory)

Returns a promise of what `storage.list` returns. Leave out `directory` to list every file of the app.

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

A save slot keeps the progress of the player under a name, such as `'slot-1'` or `'autosave'`. Each slot also keeps a small summary, such as the day or the place, so a load menu can show every slot without reading all its data.

A slot name has 1 to 64 characters, each one a letter from `a` to `z` or `A` to `Z`, a digit, a dash or an underscore. Any other name raises `The save slot name "<slot>" must use 1 to 64 letters, digits, dashes or underscores.`. The data and the summary follow the rules of [JSON values](#json-values).

### How a slot is kept

Each slot is the file `saves/<slot>.json` among the other files of the app, so `storage.list('saves')` lists it too. It holds the data, the summary, the time it was saved, the [save version](#save-versions-and-migrations) of the app, a revision that counts the writes of the slot and a checksum of the rest.

- Every write keeps the previous file of the slot as its backup, at `saves/<slot>.backup.json`. A damaged file never replaces a sound backup.
- Every write flushes the new file and its folder to the disk before and after it replaces the old file. So a crash, a dead battery or a forced quit leaves the old slot or the new one.
- When a damaged disk or a broken copy spoils the file of a slot, a read uses its backup instead. The field `recovered` of `storage.slotInfo` then reads `true`, and the log shows the warning `The save slot "<slot>" is damaged, and its backup of revision <revision> takes its place.`.
- A slot whose file and backup are both damaged raises `The save slot "<slot>" is damaged.`.
- Every write and every remove reaches the disk at once, including the browser storage of web builds.

Keep your other files out of the `saves` folder. The function `storage.listSlots` reads every file `saves/<name>.json` whose name is a valid slot name. It raises for such a file that is not a slot.

### storage.writeSlot(slot, data, summary)

Writes `data` to the slot with the current time, and replaces what the slot held. The table `summary` is optional and empty by default. A summary that is not a table, or that is a list, raises `A save summary must be a JSON object.`. The engine reports the write with the event [`saveSlotWritten`](#events).

```lua
local storage = require('haylen.storage')

local game = {day = 4, wood = 12, inventory = {'axe', 'rope'}, player = {x = 120, y = 88}}
storage.writeSlot('slot-1', game, {day = game.day, place = 'Beach'})
```

### storage.readSlot(slot)

Returns the data of the slot, or `nil` when the slot does not exist. Data of an older save version first runs through the [migrations](#storageaddmigrationfromversion-migration) up to the version of the app.

- A slot of a newer version raises `The save slot "<slot>" has version <version>, which is newer than the version <version> this app reads.`.
- A version that no chain of migrations reaches raises `The save slot "<slot>" has version <version> and no migration reaches version <version>.`.
- A slot without a sound file or backup raises `The save slot "<slot>" is damaged.`.

```lua
local storage = require('haylen.storage')

local game = storage.readSlot('slot-1')
if game then
    print('Day ' .. game.day .. ' with ' .. game.inventory[1] .. '.')
end
```

### storage.slotInfo(slot)

Returns a table that describes the slot, or `nil` when the slot does not exist. A slot without a sound file or backup raises `The save slot "<slot>" is damaged.`.

| Field | Type | Meaning |
| --- | --- | --- |
| `slot` | string | The name of the slot. |
| `savedAt` | integer | When the slot was written, in Unix seconds. |
| `summary` | table | The summary that `storage.writeSlot` received. |
| `version` | integer | The save version of the app that wrote the slot. |
| `revision` | integer | How many times the slot was written, which cloud saves compare. |
| `recovered` | boolean | `true` when the file of the slot was damaged and these fields come from its backup. |

```lua
local storage = require('haylen.storage')

local info = storage.slotInfo('slot-1')
if info then
    print(info.slot .. ' was saved on day ' .. info.summary.day .. '.')
end
```

### storage.slotExists(slot)

Returns `true` when the file of the slot exists. It does not check whether the file is sound.

```lua
local storage = require('haylen.storage')

local label = storage.slotExists('autosave') and 'Continue' or 'New game'
print(label)
```

### storage.removeSlot(slot)

Deletes the slot and its backup. Returns `true`, or `false` when the slot did not exist.

```lua
local storage = require('haylen.storage')

if storage.removeSlot('slot-2') then
    print('Slot 2 was deleted.')
end
```

### storage.listSlots()

Returns a list with a [`storage.slotInfo`](#storageslotinfoslot) table for every slot, newest first. Slots saved in the same second come in the order of their names. Backups, files in subfolders of `saves`, files that do not end in `.json` and files whose names are not slot names are skipped. A slot without a sound file or backup raises `The save slot "<slot>" is damaged.`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local storage = require('haylen.storage')

scene.push({
    enter = function(self)
        self.slots = storage.listSlots()
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        for index, info in ipairs(self.slots) do
            graphics2d.drawText(nil, info.slot .. ', day ' .. tostring(info.summary.day), 80, 60 + index * 40)
        end
    end,
})
```

### Autosave example

```lua
local scene = require('haylen.scene')
local storage = require('haylen.storage')
local timer = require('haylen.timer')

local game = storage.readSlot('autosave') or {day = 1, wood = 0}

scene.push({
    enter = function(self)
        -- The timer belongs to the scene, so it stops when the scene ends.
        timer.every(60, function()
            storage.writeSlotAsync('autosave', game, {day = game.day})
        end, {owner = self})
    end,
    exit = function(self)
        storage.writeSlot('autosave', game, {day = game.day})
    end,
})
```

## Save versions and migrations

An update of the app may change the shape of its data, such as a renamed field. The save version says which shape a slot holds. Every write stamps the version of the app, and every read migrates older data up to it, so an update never strands old saves.

### storage.setSaveVersion(version)

Sets the save version of the app, a whole number from 1. A version below 1 raises `A save version is a whole number from 1.`.

```lua
local storage = require('haylen.storage')

storage.setSaveVersion(3)
```

### storage.saveVersion()

Returns the save version of the app, which is `1` until `storage.setSaveVersion` changes it.

```lua
local storage = require('haylen.storage')

print('This app writes saves of version ' .. storage.saveVersion() .. '.')
```

### storage.addMigration(fromVersion, migration)

Registers the function that turns the data of version `fromVersion` into the data of the next version. A second call for the same version replaces the function. The function takes the data and returns the new data, and it may not yield. A version below 1 raises `A save migration needs a version from 1 and a function.`.

A read of an older slot runs every step in order on the frame thread, also for `storage.readSlotAsync`. The stored slot keeps its old version until the app writes it again.

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

## Cloud saves

A cloud save plugin moves slots between the devices of a player. It uploads the bytes that `storage.exportSlot` returns and gives downloaded bytes to `storage.importSlot`.

### storage.exportSlot(slot)

Returns the file of the slot as a string of bytes, as it is stored, or `nil` when the slot does not exist.

```lua
local async = require('async')
local events = require('haylen.events')
local platform = require('haylen.platform')
local storage = require('haylen.storage')

local cloud = platform.plugin('cloud-saves')

-- Every slot that the app writes goes up to the cloud.
events.on('saveSlotWritten', function(info)
    async.spawn(function()
        local bytes = storage.exportSlot(info.slot)
        local _, failure = cloud:call('upload', {slot = info.slot, data = platform.bytes(bytes)}):await()
        if failure then
            print('The upload failed with ' .. failure.code .. '.')
        end
    end)
end)
```

### storage.importSlot(slot, bytes, options)

Writes `bytes`, which `storage.exportSlot` returned on some device, as the slot. Returns whether it wrote them. Bytes that are not a whole slot raise `The imported save slot "<slot>" is damaged.`. The table `options` is optional.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `policy` | string | `'keepNewer'` | `'keepNewer'` imports only bytes of a higher revision than the slot of the device, or of the same revision and a later save time. A device without the slot, or with a damaged one, takes them too. So progress made on another device never disappears under an older copy. `'replace'` imports the bytes whatever the device holds. |

Another policy raises `The policy of an imported save slot must be "replace" or "keepNewer".`, and an unknown option raises `Unknown option "<key>".`. A sound file of the slot becomes its backup, as with every write. An import does not report `saveSlotWritten`.

```lua
local async = require('async')
local platform = require('haylen.platform')
local storage = require('haylen.storage')

local cloud = platform.plugin('cloud-saves')

async.spawn(function()
    local download = cloud:call('download', {slot = 'slot-1'}):await()
    if download and storage.importSlot('slot-1', download.data, {policy = 'keepNewer'}) then
        print('The cloud held newer progress.')
    end
end)
```

## Save slots on the I/O pool

These functions do what the functions of [save slots](#save-slots) do, on the I/O pool, as [calls on the I/O pool](#calls-on-the-io-pool) describes.

### storage.writeSlotAsync(slot, data, summary)

Returns a promise that resolves with `true` once the slot is written. The data and the summary are converted when you call the function, so a value that cannot become JSON raises at once. A bad slot name, or a summary that is not a table or is a list, rejects the promise. The promise settles once the slot is durable, including the browser storage of web builds. The event `saveSlotWritten` comes as it settles.

```lua
local async = require('async')
local storage = require('haylen.storage')

local game = {day = 7, wood = 30}
async.spawn(function()
    local saved, failure = storage.writeSlotAsync('autosave', game, {day = game.day}):await()
    print(saved and 'The game was saved.' or failure)
end)
```

### storage.readSlotAsync(slot)

Returns a promise of what `storage.readSlot` returns. The file is read on the I/O pool, and the [migrations](#storageaddmigrationfromversion-migration) run on the frame thread. A slot without a sound file or backup rejects the promise with `The save slot "<slot>" is damaged.`.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    local game, failure = storage.readSlotAsync('autosave'):await()
    print(game and 'Day ' .. game.day .. '.' or failure or 'There is no autosave.')
end)
```

### storage.slotInfoAsync(slot)

Returns a promise of what `storage.slotInfo` returns. A slot without a sound file or backup rejects it with `The save slot "<slot>" is damaged.`.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    local info = storage.slotInfoAsync('slot-1'):await()
    print(info and 'Revision ' .. info.revision .. '.' or 'There is no slot 1.')
end)
```

### storage.removeSlotAsync(slot)

Returns a promise that resolves with whether the slot existed, once the slot and its backup are removed and the removal is durable. A bad slot name rejects the promise.

```lua
local async = require('async')
local storage = require('haylen.storage')

async.spawn(function()
    local removed = storage.removeSlotAsync('slot-2'):await()
    print(removed and 'Slot 2 was deleted.' or 'There was no slot 2.')
end)
```

### storage.listSlotsAsync()

Returns a promise of what `storage.listSlots` returns. A slot without a sound file or backup rejects it with `The save slot "<slot>" is damaged.`.

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

## Resume state

Mobile systems end apps in the background to free memory. A player who comes back expects the app where they left it, not at the start. The resume state is a value that only the launch after such an end reads, such as the screen and the open tab.

- The app keeps its current state with `storage.setResumeState`.
- The engine writes it to the file `resume-state.json` of the storage folder when the app goes to the background, after the listeners of `appBackground` have run.
- The engine removes the file when the app comes back to the foreground or stops normally.
- So a launch after the system ended the app finds the state, and a launch after the player quit finds nothing. On the web, a page that the browser closes or discards in the background leaves the state as well.

### storage.setResumeState(value)

Keeps `value` for the next time the app goes to the background, or keeps nothing for `nil`. The value follows the rules of [JSON values](#json-values), and a value that cannot become JSON raises at once. Call it whenever what the player would want back changes, such as the screen, the open dialog or the scroll of a list. It costs nothing until the app leaves the foreground.

```lua
local storage = require('haylen.storage')

local shop = {tab = 'tools'}

function shop:selectTab(tab)
    self.tab = tab
    storage.setResumeState({screen = 'shop', tab = tab})
end
```

### storage.resumeState()

Returns the value that the run before kept, and how many seconds ago that run went to the background. Returns nothing when the run before ended normally or never reached the background. The engine reads the file once when the app starts and removes it from the disk, so a later launch never finds it again. Every call during the run returns the same values. The app decides by the age whether the state is still worth restoring. A damaged file is removed, and the log shows the warning `The resume state "resume-state.json" is damaged, so the app starts without it.`.

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

## Events

The event bus of [`haylen.events`](events.md#engine-events) reports this event of the module.

| Event | Payload | Meaning |
| --- | --- | --- |
| `saveSlotWritten` | The table `{slot, revision, version}`. | The function `storage.writeSlot` or `storage.writeSlotAsync` wrote a slot. The field `revision` is the new revision of the slot, and `version` is the save version it holds. A cloud save plugin uploads the slot here. |

```lua
local events = require('haylen.events')

events.on('saveSlotWritten', function(info)
    print('The slot ' .. info.slot .. ' reached revision ' .. info.revision .. '.')
end)
```

## Errors

These messages come from the files and their paths.

| Message | Cause |
| --- | --- |
| `The path "<path>" must be relative.` | The path starts with a slash, a backslash or a drive letter. |
| `The path "<path>" must stay inside its root folder.` | A `..` segment leaves the storage folder. |
| `A storage path cannot be empty.` | The path is empty, or names the storage folder itself, such as `'.'`. |
| `The storage path "<path>" ends with ".tmp", which the storage keeps for the temporary files of its writes.` | The path has the `.tmp` extension. |
| `The storage file "<path>" was not found.` | The file to read does not exist. |
| `The storage file "<path>" could not be read.` | The system refused to read the file. |
| `The storage file "<path>" could not be written.` | The write failed, such as over a folder or below a path that names a file. |
| `The storage folder "<directory>" could not be listed, and the file system reported "<reason>".` | A folder below `directory` cannot be read. |
| `The user storage could not be listed, and the file system reported "<reason>".` | A folder cannot be read while the whole storage is listed. |
| `[json.exception.parse_error.101] <details>` | The file that `storage.readJson` or `storage.readJsonAsync` read is not valid JSON. |

These messages come from the conversion of [JSON values](#json-values).

| Message | Cause |
| --- | --- |
| `A <type> cannot be converted to JSON.` | The value holds a function, a thread or a userdata. |
| `A <type> key cannot be converted to JSON.` | A table has a key that is not a string or a number. |
| `A string that is not valid UTF-8 cannot be converted to JSON.` | The value holds a string that is not valid UTF-8. |
| `A key that is not valid UTF-8 cannot be converted to JSON.` | A table has a key that is not valid UTF-8. |
| `A number that is not finite cannot be converted to JSON.` | The value holds NaN or an infinity. |
| `Value is nested too deeply to convert to JSON.` | Tables nest deeper than 128 levels, or a table contains itself. |
| `JSON value is nested too deeply to push to Lua.` | The JSON read back nests deeper than 128 levels. |

These messages come from the save slots.

| Message | Cause |
| --- | --- |
| `The save slot name "<slot>" must use 1 to 64 letters, digits, dashes or underscores.` | The slot name is empty, too long or has other characters. |
| `A save summary must be a JSON object.` | The summary is not a table, or it is a list. |
| `The save slot "<slot>" is damaged.` | The file of the slot and its backup are not valid JSON, lack a field or fail their checksum. |
| `The save slot "<slot>" has version <version>, which is newer than the version <version> this app reads.` | A newer version of the app wrote the slot. |
| `The save slot "<slot>" has version <version> and no migration reaches version <version>.` | A step of the migrations is missing. |
| `A save version is a whole number from 1.` | The function `storage.setSaveVersion` received a version below 1. |
| `A save migration needs a version from 1 and a function.` | The function `storage.addMigration` received a version below 1. |
| `The imported save slot "<slot>" is damaged.` | The function `storage.importSlot` received bytes that are not a whole slot. |
| `The policy of an imported save slot must be "replace" or "keepNewer".` | The function `storage.importSlot` received another policy. |
| `Unknown option "<key>".` | The options of `storage.importSlot` hold a key that it does not take. |
