# 💾 Storage

This guide explains how an app keeps data between sessions with the context `haylen::storage` and its Lua module `haylen.storage`. It covers the private folder of each app, plain files and when to read them on the I/O pool, save slots with their backups, versions and migrations, copies of slots that move between devices, the moments when data becomes durable, and the resume state that brings the player back after the system ended the app. The [`haylen.storage` reference](../lua-api/storage.md) lists every function, rule and error.

## What goes where

An app holds data in a few places, and each one has its own job. The package of the app is read only, and the storage folder is where the app writes.

| Data | Where it lives | Read it with |
| --- | --- | --- |
| Art, sounds, levels and scripts that ship with the app. | The package. | [`haylen.assets`](../lua-api/assets.md) |
| Choices of the player, such as the volume, the controls or the language. | The preferences file of the storage folder. | [`haylen.preferences`](../lua-api/preferences.md) |
| The progress of a game or the documents of an app. | Save slots of the storage folder. | [Save slots](../lua-api/storage.md#save-slots) of `haylen.storage` |
| Caches, logs, replays and exported data. | Plain files of the storage folder. | [Files](../lua-api/storage.md#files) of `haylen.storage`, or the `fs` module of Varn |
| The screen the player was on when the system ended the app. | The resume state of the storage folder. | [Resume state](../lua-api/storage.md#resume-state) of `haylen.storage` |

Every app has a storage folder of its own, named after the `identifier` of its `haylen.json`, in the place that each platform gives apps for their data. The [storage folder](../lua-api/storage.md#the-storage-folder) section lists that place on every platform. Two apps never share a folder, and an app without an identifier gets one that the tooling or the engine makes from where the app runs, so its files stay apart too.

## Files

The file functions take paths relative to the storage folder, such as `'cache/map.json'`, and refuse a path that would leave it. A write never changes a file in place. It writes a temporary file beside it and then moves that file over the old one, so a crash in the middle of a write leaves the old content and never half of the new one. Missing folders appear on their own.

Text and JSON are the two forms. A text is any Lua string, binary bytes included. A JSON file holds a Lua value that the module converts on the way in and out, by the rules of [JSON values](../lua-api/storage.md#json-values). Those rules raise an error at the moment of the write for a value that JSON cannot hold, such as a function or a `Vec2`, so a broken value never reaches the disk and spoils a later read.

```lua
local storage = require('haylen.storage')

local stats = {runs = 0, bestTime = nil}
if storage.exists('stats.json') then
    stats = storage.readJson('stats.json')
end

stats.runs = stats.runs + 1
storage.writeJson('stats.json', stats)
storage.writeText('logs/last-run.txt', 'Run ' .. stats.runs .. ' finished.')
print(#storage.list('logs') .. ' log files.')
```

### Frame thread or I/O pool

Every file and slot function comes in two forms. The plain form, such as `storage.readJson`, works on the frame thread and returns when the disk is done. It suits small files read at a calm moment, such as the start of a scene. The form with the suffix `Async`, such as `storage.readJsonAsync`, does the same work on the I/O pool, a set of threads for slow input and output, and returns a promise at once. Use it for large files and for saves in the middle of play, so the frame never waits for the disk.

The calls on the I/O pool run one at a time in the order they started, so a read that starts after a write always sees it. The [calls on the I/O pool](../lua-api/storage.md#calls-on-the-io-pool) section says how their failures reach the app, and the [Lua guide](lua.md#asynchronous-code) explains tasks and promises.

```lua
local async = require('async')
local storage = require('haylen.storage')

local level = {seed = 9, tiles = {}}
for index = 1, 4096 do
    level.tiles[index] = index % 7
end

async.spawn(function()
    local written, failure = storage.writeJsonAsync('cache/level-9.json', level):await()
    if not written then
        print('The cache was not written: ' .. failure)
        return
    end
    local copy = storage.readJsonAsync('cache/level-9.json'):await()
    print(#copy.tiles) -- 4096
end)
```

For streaming, for folders of their own and for very large files, the `fs` module of Varn reads and writes the same folder from the path that [`storage.root`](../lua-api/storage.md#storageroot) returns. The [disk access](lua.md#disk-access) section of the Lua guide compares the two modules.

## Save slots

A save slot keeps one save of the player under a name, such as `'slot-1'` or `'autosave'`. It holds the data, which is any JSON value, and a small summary, such as the day, the place or the play time. A load menu reads only the summaries through [`storage.listSlots`](../lua-api/storage.md#storagelistslots), newest first, and reads the data of one slot only when the player picks it.

```lua
local storage = require('haylen.storage')

local game = {day = 12, place = 'Lighthouse', inventory = {'lamp', 'rope'}}
storage.writeSlot('slot-2', game, {day = game.day, place = game.place})

for _, info in ipairs(storage.listSlots()) do
    local date = os.date('%Y-%m-%d %H:%M', info.savedAt)
    print(string.format('%s: day %d at the %s, saved %s.', info.slot, info.summary.day, info.summary.place, date))
end

local loaded = storage.readSlot('slot-2')
print(loaded and loaded.inventory[1]) -- lamp
```

A slot is safer than a plain file in three ways. Each write reaches the disk before and after the new file replaces the old one, so a crash, a forced quit or a dead battery leaves the old save or the new one. Each write keeps the file before it as a backup, so when a damaged disk spoils a slot, a read takes the backup and [`storage.slotInfo`](../lua-api/storage.md#storageslotinfoslot) reports `recovered`. And each slot carries a checksum, so damage is found instead of read as data. The section [how a slot is kept](../lua-api/storage.md#how-a-slot-is-kept) tells the details.

An autosave usually writes on the I/O pool at intervals and at the moments that matter, such as the end of a level or the event `appBackground`. Give its timer an `owner`, so it stops with the scene that runs the game.

```lua
local events = require('haylen.events')
local scene = require('haylen.scene')
local storage = require('haylen.storage')
local timer = require('haylen.timer')

local game = storage.readSlot('autosave') or {day = 1, coins = 0}

local function save()
    storage.writeSlotAsync('autosave', game, {day = game.day})
end

scene.push({
    enter = function(self)
        timer.every(120, save, {owner = self})
        events.on('appBackground', save, {owner = self})
    end,
    finishDay = function(self)
        game.day = game.day + 1
        save()
    end,
})
```

## Versions and migrations

An update of the app often changes the shape of its saves, such as a field that splits in two or a list that becomes a table. The save version tells which shape a slot holds. The app declares its version with [`storage.setSaveVersion`](../lua-api/storage.md#storagesetsaveversionversion), every write stamps it, and a read of an older slot runs the migrations from its version up to the current one before it returns the data.

A migration turns the data of one version into the data of the next one, so each update adds one small step and never rewrites the steps before it. A slot saved three versions ago passes through the three steps in order. Register the migrations once, before the first read, such as in an autoload.

```lua
local storage = require('haylen.storage')

storage.setSaveVersion(3)

-- Version 2 keeps the coins in a wallet.
storage.addMigration(1, function(data)
    data.wallet = {coins = data.coins or 0, gems = 0}
    data.coins = nil
    return data
end)

-- Version 3 counts the days from 1 instead of 0.
storage.addMigration(2, function(data)
    data.day = data.day + 1
    return data
end)

local game = storage.readSlot('autosave')
if game then
    print('Day ' .. game.day .. ' with ' .. game.wallet.coins .. ' coins.')
end
```

A slot that a newer version of the app wrote raises an error instead of losing what the newer version knows, which matters when a player keeps an old version on one device. A version with no migration that leads on raises an error too, so a forgotten step shows in the first test instead of in the hands of players.

## Copies on other devices

A player who plays on a phone and a tablet expects one progress. The engine does not upload anything itself, because every store and service has its own cloud. A plugin of cloud saves moves the slots. It takes the bytes of a slot with [`storage.exportSlot`](../lua-api/storage.md#storageexportslotslot) and gives the bytes it downloaded to [`storage.importSlot`](../lua-api/storage.md#storageimportslotslot-bytes-options).

The bytes are the slot as it is stored, checksum and revision included. The revision counts the writes of a slot, and the default policy `keepNewer` of an import takes the downloaded copy only when it is newer than the copy of the device. So progress made on the device is never replaced by an older copy from the cloud. The event `saveSlotWritten` of the [events](../lua-api/storage.md#events) section tells the plugin when a slot changed and should go up.

```lua
local async = require('async')
local events = require('haylen.events')
local platform = require('haylen.platform')
local storage = require('haylen.storage')

local cloud = platform.plugin('cloud-saves')

events.on('saveSlotWritten', function(info)
    async.spawn(function()
        local bytes = storage.exportSlot(info.slot)
        cloud:call('upload', {slot = info.slot, data = platform.bytes(bytes)}):await()
    end)
end)

local function pull(slot)
    async.spawn(function()
        local download = cloud:call('download', {slot = slot}):await()
        if download and storage.importSlot(slot, download.data) then
            print('The slot ' .. slot .. ' came from another device.')
        end
    end)
end

pull('autosave')
```

## When data is durable

A write on most platforms is on the disk when it returns. A browser is different: the files of a page live in memory and reach IndexedDB only when the engine copies them there. [`storage.flush`](../lua-api/storage.md#storageflush) makes that copy, and every write of a save slot does it by itself. On the other platforms a flush returns at once, so calling it after an important save costs nothing where it is not needed.

The engine also flushes when the app goes to the background, after the listeners of `appBackground` ran, so what those listeners save is durable too. A mobile system may end an app in the background without a word, which makes `appBackground` the last safe moment to save. The [lifecycle guide](lifecycle.md#app-states) describes what happens on each platform, and how a page that the browser hides or freezes saves.

On the web, the user data of a site belongs to one tab at a time. A second tab still runs the app, but its storage refuses every call with a sentence that says why, as [a tab without its user data](../lua-api/storage.md#a-tab-without-its-user-data) describes. An app that must run in that tab catches the error and goes on without saving.

## The resume state

Phones and tablets end apps in the background to free memory. The player who comes back sees the app start from scratch, unless the app brings back what they saw. The resume state does that. The app keeps a small value with [`storage.setResumeState`](../lua-api/storage.md#storagesetresumestatevalue) whenever what the player would want back changes, such as the screen, the tab or the open item. The engine writes it only when the app goes to the background and removes it when the app comes back or quits normally.

So at the next start, [`storage.resumeState`](../lua-api/storage.md#storageresumestate) returns a value only when the system ended the app, and nothing after a normal quit. It also returns how long ago the app went to the background, and the app decides whether the state is still worth bringing back. A player who left a shop open a week ago probably wants the menu instead.

```lua
local scene = require('haylen.scene')
local storage = require('haylen.storage')

local Inventory = require('scenes.inventory')
local Title = require('scenes.title')

local function openInventory(page)
    scene.replace(Inventory(page))
    storage.setResumeState({screen = 'inventory', page = page})
end

local saved, age = storage.resumeState()
if saved and saved.screen == 'inventory' and age < 1800 then
    openInventory(saved.page)
else
    scene.push(Title())
    storage.setResumeState({screen = 'title'})
end
```

The resume state is not a save. It brings back a place, never progress, so the progress still goes into a slot. An app whose screens live in the navigation of `haylen.ui` gets this for its screens from the router, which keeps the locations of the navigation in the same way, as the [navigation guide](../ui/navigation.md) explains. The [lifecycle guide](lifecycle.md) lists `appStarted` with the resume state among the moments of the app.

## From C++

The context `haylen::storage` holds the same parts for C++ apps and plugins. `storage::UserStorage` is the storage folder with atomic reads and writes, which `core::Engine::getStorage` returns. The storage plugin, `plugins::StoragePlugin`, owns the `storage::SaveSlots`, the `storage::Preferences` and the `storage::ResumeState` of the app and runs their background work on the I/O pool one operation at a time. The Lua module is a thin layer over these classes, so a C++ app and a Lua app read and write the same files.

```cpp
#include "haylen/core/Engine.hpp"
#include "haylen/plugins/StoragePlugin.hpp"

namespace haylen::app {

class Progress final {
  public:
    static void saveDay(core::Engine& engine, int day) {
        storage::SaveSlots& slots = engine.getPlugin<plugins::StoragePlugin>().getSaveSlots();
        core::Json data = {{"day", day}};
        core::Json summary = {{"day", day}};
        slots.write("autosave", data, summary);
        engine.getStorage().writeText("logs/last-day.txt", "Day " + std::to_string(day) + ".");
    }
};

}
```
