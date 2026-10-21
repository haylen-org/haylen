# 📖 `haylen.preferences`

The module `haylen.preferences` keeps the choices of the player between sessions, such as the volumes, the language, the difficulty and rebound controls. Use it for values that belong to the player rather than to one playthrough. The progress of the app belongs in the save slots of [`haylen.storage`](storage.md#save-slots) instead.

```lua
local preferences = require('haylen.preferences')
```

## How preferences are kept

The preferences are one table of values, which the engine keeps as the JSON file `preferences.json` in the [storage folder](storage.md#the-storage-folder) of the app. You read and change them in memory, and they reach the disk when you save them.

- A key is a dotted path into nested tables. So `preferences.set('audio.volume.music', 0.5)` stores `{audio = {volume = {music = 0.5}}}`, and `preferences.get('audio.volume')` returns the table `{music = 0.5}`.
- A key with an empty part, such as `'audio..music'` or `'.audio'`, raises `The preference key "<key>" must be a dotted path without empty parts.`.
- The values follow the rules of the [JSON values](storage.md#json-values) of `haylen.storage`. A value that JSON cannot hold raises at once, so the preferences never hold a value that could not be saved.

The engine loads the preferences when it starts. A damaged file leaves the preferences empty, and the log shows the warning `The preferences file "preferences.json" is damaged. The app starts with default preferences, and the next save replaces the file.`. The engine also saves unsaved changes on its own in two cases. It saves them when the app stops. It saves them when the app goes to the background, after the listeners of the `appBackground` event of [`haylen.events`](events.md) have run, so what they change is saved too.

## Values

### preferences.get(key, default)

Returns the value stored under `key`, or `default` when nothing is stored there. The argument `default` is optional and `nil` by default. A table that it returns is a copy, so change it and store it again with `preferences.set`.

```lua
local preferences = require('haylen.preferences')

local difficulty = preferences.get('game.difficulty', 'normal')
local volumes = preferences.get('audio.volume', {})
print('The difficulty is ' .. difficulty .. '.')
print(volumes.music)
```

### preferences.set(key, value)

Stores `value` under `key` and marks the preferences as changed. Missing tables along the path are created. Storing `nil` removes the key the same way as `preferences.remove`, so `preferences.has` returns `false` and `preferences.get` returns the default again.

A path that passes through a stored value that is not a table raises `The preference key "<key>" passes through "<part>", which holds a value instead of a group.`. A value or a key that JSON cannot hold raises the errors of the [JSON values](storage.md#json-values). After an error, the preferences stay as they were.

```lua
local preferences = require('haylen.preferences')

preferences.set('game.difficulty', 'hard')
preferences.set('game.subtitles', true)
preferences.set('video.scale', 2)
preferences.set('recent', {'slot-1', 'slot-3'})
preferences.set('video.scale', nil)
```

### preferences.has(key)

Returns `true` when a value or a group is stored under `key`.

```lua
local preferences = require('haylen.preferences')

if not preferences.has('game.language') then
    print('Ask the player for a language.')
end
```

### preferences.remove(key)

Removes the value or the group stored under `key` and returns `true`. Returns `false` when nothing was stored there. Only a removal marks the preferences as changed.

```lua
local preferences = require('haylen.preferences')

if preferences.remove('video') then
    print('The video settings are back to their defaults.')
end
```

### preferences.clear()

Removes every preference and marks the preferences as changed. Call `preferences.save` to erase them from the disk too.

```lua
local preferences = require('haylen.preferences')

preferences.clear()
preferences.save()
```

### preferences.values()

Returns every preference as one table. The table is a copy.

```lua
local preferences = require('haylen.preferences')

for group, values in pairs(preferences.values()) do
    print(group, values)
end
```

## Saving and loading

### preferences.save()

Writes the preferences to `preferences.json` at once, with an indentation of two spaces, and clears the changed flag. On the web it also makes the file durable, as `storage.flush` does.

```lua
local preferences = require('haylen.preferences')

preferences.set('game.difficulty', 'easy')
preferences.save()
```

### preferences.load()

Replaces the preferences in memory with the stored file and drops unsaved changes. When no file exists yet, the preferences become empty.

- A file that is not a JSON object empties the preferences and raises `The preferences file "preferences.json" is damaged.`.
- A file that cannot be read raises `The storage file "preferences.json" could not be read.`. The preferences in memory stay as they were, unsaved changes included.

```lua
local preferences = require('haylen.preferences')

preferences.set('game.difficulty', 'impossible')
preferences.load()
print(preferences.get('game.difficulty'))
```

### preferences.dirty()

Returns `true` when the preferences changed since they were last loaded or saved.

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

## Engine preferences

Some choices of the player live in the engine itself: the volumes of the audio buses, the full screen and the action map. These two functions copy them into the preferences and back.

| Key | Value |
| --- | --- |
| `audio.volume` | A table with the volume of every audio bus, under the name of the bus. Read it with `preferences.get('audio.volume.music')`, or with `preferences.get('audio.volume')['voice.lines']` for a bus whose name holds a dot. |
| `audio.muted` | A table with whether every audio bus is muted, under the name of the bus. |
| `window.fullscreen` | Whether the window is in full screen. |
| `input.actions` | The action map, in the format of [`input.saveActions`](input.md#inputsaveactions). |

### preferences.capture()

Stores the engine preferences under the keys of the table above. Buses that the app has not created keep the values stored for them. It does not write the file, so call `preferences.save` after it. A stored `audio.volume` or `audio.muted` that is not a table raises `The preference "<key>" has a value of the wrong type.`.

```lua
local audio = require('haylen.audio')
local preferences = require('haylen.preferences')
local window = require('haylen.window')

audio.setBusVolume('music', 0.4)
audio.setBusMuted('sfx', false)
window.setFullscreen(true)
preferences.capture()
preferences.save()
```

### preferences.apply()

Applies the engine preferences that are stored under the keys of the table above. Call it at startup, after you create your own buses and load the default action map, so the stored choices override the defaults.

- A missing key leaves the current state alone.
- Buses that the app has not created yet are skipped.
- A stored action map replaces the current one. An invalid one raises the errors of [`input.loadActions`](input.md#inputloadactionsdocument).
- A stored value of the wrong type raises `The preference "<key>" has a value of the wrong type.`.
- A bus value of the wrong type in `audio.volume` or `audio.muted` raises `The preference "<key>" has a value of the wrong type for the bus "<bus>".`.

```lua
local audio = require('haylen.audio')
local input = require('haylen.input')
local preferences = require('haylen.preferences')

audio.createBus('voices')
input.loadActions('input.json')
preferences.apply()
```

## Options menu example

```lua
local audio = require('haylen.audio')
local input = require('haylen.input')
local preferences = require('haylen.preferences')
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
| `The preference key "<key>" must be a dotted path without empty parts.` | The key is empty or has an empty part. |
| `The preference key "<key>" passes through "<part>", which holds a value instead of a group.` | The function `preferences.set` would have to replace a stored value with a group. |
| `The preferences file "preferences.json" is damaged.` | The function `preferences.load` found a file that is not a JSON object. |
| `The storage file "preferences.json" could not be read.` | The function `preferences.load` could not read the file. |
| `The preference "<key>" has a value of the wrong type.` | The function `preferences.apply` or `preferences.capture` found an engine key with a value of another type. |
| `The preference "<key>" has a value of the wrong type for the bus "<bus>".` | The function `preferences.apply` found a bus in `audio.volume` or `audio.muted` with a value of another type. |
| `A <type> cannot be converted to JSON.` | The value of `preferences.set` or the default of `preferences.get` holds a function, a thread or a userdata. |
| `A <type> key cannot be converted to JSON.` | The value of `preferences.set` holds a table with a key that is not a string or a number. |
| `A string that is not valid UTF-8 cannot be converted to JSON.` | The value of `preferences.set` holds a string that is not valid UTF-8. |
| `A key that is not valid UTF-8 cannot be converted to JSON.` | The key of `preferences.set`, or a key of a table in its value, is not valid UTF-8. |
| `A number that is not finite cannot be converted to JSON.` | The value of `preferences.set` holds NaN or an infinity. |
| `Value is nested too deeply to convert to JSON.` | The value of `preferences.set` nests tables deeper than 128 levels, or a table contains itself. |
