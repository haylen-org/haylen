# haylen.preferences

Player preferences that persist between sessions, such as volumes, language, difficulty and rebound controls. Use it for values that belong to the player rather than to one playthrough, and use the save slots of [haylen.storage](storage.md) for app progress.

```lua
local preferences = require('haylen.preferences')
```

## Keys and storage

Preferences are one JSON object kept in `preferences.json` in the private user storage of the app. Keys are dotted paths into nested tables, so `preferences.set('audio.volume.music', 0.5)` stores `{audio = {volume = {music = 0.5}}}` and `preferences.get('audio.volume')` returns the table `{music = 0.5}`. A key with an empty part, such as `'audio..music'` or `'.audio'`, raises `The preference key '<key>' must be a dotted path without empty parts.`.

Values are converted to JSON the same way as the data of the save slots of [haylen.storage](storage.md): booleans, numbers, strings and tables of them are kept, and sequences become lists.

The engine loads the preferences when it starts. A damaged file is logged as a warning, the app starts with empty preferences and the next save replaces the file. Changes stay in memory until `preferences.save()`, and the engine also saves unsaved changes when the app is suspended and when it stops.

## Functions

### preferences.get(key, default)

Returns the value stored under the key, or `default` when nothing is stored there. The default is optional and defaults to `nil`.

```lua
local preferences = require('haylen.preferences')

local difficulty = preferences.get('game.difficulty', 'normal')
local volumes = preferences.get('audio.volume', {})
print(difficulty, volumes.music)
```

### preferences.set(key, value)

Stores a value under the key and marks the preferences as changed. Missing tables along the path are created. Storing `nil` removes the key the same way `preferences.remove()` does, so `preferences.has()` reports false and `preferences.get()` returns the default again. A path that passes through a stored value that is not a table raises `The preference key <key> passes through <part>, which holds a value instead of a group.`.

```lua
local preferences = require('haylen.preferences')

preferences.set('game.difficulty', 'hard')
preferences.set('game.subtitles', true)
preferences.set('video.scale', 2)
preferences.set('recent', {'slot-1', 'slot-3'})
preferences.set('video.scale', nil)
```

### preferences.has(key)

Returns true when a value or a group is stored under the key.

```lua
local preferences = require('haylen.preferences')

if not preferences.has('game.language') then
    print('ask the player for a language')
end
```

### preferences.remove(key)

Removes the value or group stored under the key and returns true, or returns false when nothing was stored there.

```lua
local preferences = require('haylen.preferences')

preferences.remove('video')
```

### preferences.clear()

Removes every preference and marks the preferences as changed. Call `preferences.save()` to erase them from storage too.

```lua
local preferences = require('haylen.preferences')

preferences.clear()
preferences.save()
```

### preferences.save()

Writes the preferences to `preferences.json` at once and clears the changed flag.

```lua
local preferences = require('haylen.preferences')

preferences.set('game.difficulty', 'easy')
preferences.save()
```

### preferences.load()

Replaces the preferences in memory with the stored file, discarding unsaved changes, or empties them when no file exists yet. A file that is not a JSON object empties the preferences and raises `The preferences file 'preferences.json' is damaged.`.

```lua
local preferences = require('haylen.preferences')

preferences.set('game.difficulty', 'impossible')
preferences.load()
print(preferences.get('game.difficulty'))
```

### preferences.dirty()

Returns true when the preferences changed since they were last loaded or saved.

```lua
local preferences = require('haylen.preferences')
local scene = require('haylen.scene')

scene.push({
    exit = function(self)
        if preferences.dirty() then
            preferences.save()
        end
    end,
})
```

### preferences.values()

Returns every preference as one table.

```lua
local preferences = require('haylen.preferences')

for group, values in pairs(preferences.values()) do
    print(group, values)
end
```

## Engine preferences

### preferences.capture()

Stores the engine preferences under these keys. It does not save them to storage, so call `preferences.save()` afterwards.

| Key | Value |
| --- | --- |
| `audio.volume.<bus>` | The volume of every audio bus, such as `audio.volume.music`. |
| `audio.muted.<bus>` | Whether every audio bus is muted. |
| `window.fullscreen` | Whether the window is fullscreen. |
| `input.actions` | The action map, in the format of `input.saveActions()`. |

```lua
local preferences = require('haylen.preferences')
local audio = require('haylen.audio')
local window = require('haylen.window')

audio.setBusVolume('music', 0.4)
audio.setBusMuted('sfx', false)
window.setFullscreen(true)
preferences.capture()
preferences.save()
```

### preferences.apply()

Applies the engine preferences that are stored under the keys of `preferences.capture()`. Missing keys leave the current state alone, buses the app has not created yet are skipped, and a stored action map replaces the current one or raises the errors of `input.loadActions()` when it is invalid. A stored value of the wrong type raises `The preference '<key>' has a value of the wrong type.`. Call it at startup after creating custom buses and loading the default action map, so stored choices override the defaults.

```lua
local preferences = require('haylen.preferences')
local audio = require('haylen.audio')
local input = require('haylen.input')

audio.createBus('voices')
input.loadActions('input.json')
preferences.apply()
```

## Options menu example

```lua
local preferences = require('haylen.preferences')
local audio = require('haylen.audio')
local input = require('haylen.input')
local scene = require('haylen.scene')

preferences.apply()

scene.push({
    update = function(self, dt)
        local volume = audio.busVolume('music')
        if input.keyPressed('right') then
            audio.setBusVolume('music', math.min(1, volume + 0.1))
        elseif input.keyPressed('left') then
            audio.setBusVolume('music', math.max(0, volume - 0.1))
        end
        if input.keyPressed('m') then
            audio.setBusMuted('music', not audio.busMuted('music'))
        end
    end,
    exit = function(self)
        preferences.capture()
        preferences.save()
    end,
})
```

## Errors

| Message | Cause |
| --- | --- |
| `The preference key '<key>' must be a dotted path without empty parts.` | The key is empty or has an empty part. |
| `The preference key <key> passes through <part>, which holds a value instead of a group.` | `preferences.set()` would have to replace a stored value with a group. |
| `The preferences file 'preferences.json' is damaged.` | `preferences.load()` found a file that is not a JSON object. |
| `The preference '<key>' has a value of the wrong type.` | `preferences.apply()` found an engine key with a value of another type. |
| `A <type> cannot be converted to JSON.` | `preferences.set()` or `preferences.get()` received a function, userdata or thread. |
