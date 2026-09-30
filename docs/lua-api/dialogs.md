# haylen.dialogs

`haylen.dialogs` shows the native dialogs of the system: message boxes with up to three buttons, and pickers of files to open, of the destination of data to save and of a folder. They look and behave like the dialogs of every other app of the platform, which suits desktop tools, editors and settings that import or export files. Menus and messages drawn in the style of the app belong to [haylen.ui](ui.md) instead.

```lua
local dialogs = require('haylen.dialogs')
```

A dialog never blocks the app. Every function shows its dialog at once and returns a [call](#calls), the same kind of object `platform.call` of [haylen.platform](platform.md#calls) returns, and the choice of the user reaches Lua at the start of a later frame, where `call:await()` inside a coroutine returns it. A dialog that the user dismissed or cancelled returns `nil`, and a dialog that failed returns `nil` and an [error](#errors). A platform that has no such dialog fails the call with the code `unsupported`, never with a default choice. Options are tables whose unknown keys raise `Unknown option '<key>'.`, and every function takes a `timeout` of its own.

## Functions

### dialogs.message(options)

Shows a message with one to three buttons and returns a call whose `await` gives the button the user pressed, counted from 1 in the order of `buttons`, or `nil` when the user dismissed the message without a button, such as with Escape or the back button.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | string | `''` | The title of the message. |
| `text` | string | Required | The message. |
| `kind` | string | `'info'` | `'info'`, `'warning'` or `'error'`, which picks the icon or the style where the platform has them. |
| `buttons` | table | Required | One to three button labels in the language of the app. |
| `timeout` | number | None | Seconds after which the call fails with the code `timeout` and the platform closes the dialog. |

A missing text raises `A message dialog needs a text.`, a list of buttons that is empty or longer than three raises `A message dialog has one to three buttons, not <count>.`, and an empty label raises `Every button of a message dialog needs a label.`.

```lua
local dialogs = require('haylen.dialogs')
local scene = require('haylen.scene')

local Editor = {}

function Editor:enter()
    scene.spawn(self, function()
        local button = dialogs.message({title = 'Unsaved map', text = 'Save the changes before leaving?', kind = 'warning', buttons = {'Save', 'Discard', 'Cancel'}}):await()
        if button == 1 then
            print('saving')
        elseif button == 2 then
            print('leaving without saving')
        end
    end)
end

scene.push(Editor)
```

### dialogs.openFiles(options)

Shows a picker of files to open and returns a call whose `await` gives a list of the picked files, each a table with `name` and `path`, or `nil` when the user cancelled. The list holds one file unless `multiple` is `true`. Every file is readable at its `path` with the `fs` module of Varn: on desktops it is the file itself, and on phones, tablets and the web it is a copy under `tmp/dialogs` in the folder of [haylen.storage](storage.md), which the engine empties whenever an app starts.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | string | `''` | The title of the picker, where the platform shows one. |
| `filters` | table | Every file | A list of file types, each a table with a `name` and a list of `extensions` without their dot, such as `{name = 'Maps', extensions = {'tmj', 'json'}}`. |
| `multiple` | boolean | `false` | Whether the user may pick several files. |
| `timeout` | number | None | Seconds after which the call fails with the code `timeout` and the platform closes the picker. |

A filter that is not a table with a name and a list of extensions raises `The filters of a file dialog are a list of tables with a name and a list of extensions.`, a filter without a name raises `Every filter of a file dialog needs a name.`, a filter without extensions raises `The filter '<name>' needs at least one extension.`, and an extension with a leading dot, a wildcard, a separator or a space raises `The extension '<extension>' of the filter '<name>' is invalid. Extensions come without their dot, such as png or tar.gz.`.

```lua
local async = require('async')
local dialogs = require('haylen.dialogs')
local fs = require('fs')

async.spawn(function()
    local files, err = dialogs.openFiles({title = 'Import maps', filters = {{name = 'Tiled maps', extensions = {'tmj', 'json'}}}, multiple = true}):await()
    if err then
        print('no picker here: ' .. err)
        return
    end
    for _, file in ipairs(files or {}) do
        print(file.name .. ' has ' .. #fs.readFile(file.path):await() .. ' bytes')
    end
end)
```

### dialogs.saveFile(options)

Shows a picker of the destination of a file, writes `data` there and returns a call whose `await` gives the saved file, a table with `name` and, where the platform gives one, `path`, or `nil` when the user cancelled. The web saves a download, which has no path, or a file of the file system where the browser lets the page pick one.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | string | `''` | The title of the picker, where the platform shows one. |
| `name` | string | Required | The file name the picker suggests, without folders. |
| `data` | string | Required | The bytes to write, which may be empty. |
| `filters` | table | Every file | File types as `dialogs.openFiles` takes them. |
| `timeout` | number | None | Seconds after which the call fails with the code `timeout` and the platform closes the picker. |

A missing name raises `A save dialog needs the name it suggests for the file.`, a name with a folder raises `The name '<name>' that a save dialog suggests is a file name, without folders.`, and missing data raises `A save dialog needs the data it writes, as a string.`.

```lua
local async = require('async')
local dialogs = require('haylen.dialogs')
local json = require('json')

async.spawn(function()
    local saved = dialogs.saveFile({title = 'Export the save', name = 'island-save.json', data = json.encode({gold = 120, day = 7}), filters = {{name = 'Saves', extensions = {'json'}}}}):await()
    if saved then
        print('exported to ' .. (saved.path or saved.name))
    end
end)
```

### dialogs.openFolder(options)

Shows a picker of a folder and returns a call whose `await` gives the path of the folder, or `nil` when the user cancelled. `options` is optional.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | string | `''` | The title of the picker, where the platform shows one. |
| `timeout` | number | None | Seconds after which the call fails with the code `timeout` and the platform closes the picker. |

```lua
local async = require('async')
local dialogs = require('haylen.dialogs')

async.spawn(function()
    local folder, err = dialogs.openFolder({title = 'Export the level to'}):await()
    if err and err.code == 'unsupported' then
        print('this platform picks no folders, so the level goes to the storage of the app')
    elseif folder then
        print('exporting to ' .. folder)
    end
end)
```

## Calls

Every function returns a `haylen.PlatformCall`, whose members work as for [platform calls](platform.md#calls).

| Member | Meaning |
| --- | --- |
| `call:await()` | Waits inside a coroutine and returns the choice of the user, or `nil` and the [error](#errors) of the dialog. Awaiting a call that already settled returns at once. |
| `call:cancel()` | Closes the dialog where the platform can and fails the call with the code `cancelled` at the start of the next frame. Returns `true` when the dialog was still open and `false` when it had already settled. |
| `call.id` | The id of the dialog, unique in the process, so an answer that arrives after the app restarted never answers a dialog of the new app. |
| `call.done` | Whether the call has settled. |
| `call.promise` | The Varn promise of the call, for the combinators of `async`, where a failure is only its message. |

A dialog that is still open when the app stops, such as for a hot reload, closes, and its call never settles.

```lua
local async = require('async')
local dialogs = require('haylen.dialogs')

local question = dialogs.message({text = 'Keep the new graphics settings?', buttons = {'Keep', 'Revert'}, timeout = 15})

async.spawn(function()
    local button, err = question:await()
    if button ~= 1 then
        print(err and err.code == 'timeout' and 'no answer, reverting' or 'reverting')
    end
end)

local function leaveSettings()
    question:cancel()
end
```

## Errors

A failed dialog returns the same error table as [platform calls](platform.md#errors), with `message`, `code` and `data`, which reads as its message in `tostring` and in string concatenation.

| Code | When |
| --- | --- |
| `unsupported` | The platform has no such dialog, with a message that says why. |
| `cancelled` | `call:cancel()` gave the dialog up. |
| `timeout` | The `timeout` of the dialog passed first. |
| `failed` | The platform could not show the dialog or write the file, with its reason as the message. |

## Platforms

| Platform | Message | Open files | Save file | Open folder |
| --- | --- | --- | --- | --- |
| macOS | A sheet on the window of the app. | A sheet on the window of the app. | A sheet on the window of the app. | A sheet on the window of the app. |
| iOS, iPadOS and Mac Catalyst | An alert. | The document picker, which copies the files. | The document picker, which exports the data. | The document picker, which keeps access to the folder. |
| tvOS | An alert. | `unsupported` | `unsupported` | `unsupported` |
| Android | An alert with up to three buttons. | The document picker of the system, which copies the files. | The document creator of the system, which never replaces a file and adds a number to a taken name. | `unsupported`, because Android folders have no paths. |
| Web | A dialog over the canvas in the colors of the page. Its first button takes the focus, Tab and the arrows move between the buttons, and Escape dismisses it. | The file picker of the browser, which copies the files. | The save picker of the browser where it has one, and a download elsewhere. | `unsupported`, because browsers give pages no paths of folders. |
| Windows | A task dialog owned by the window of the app. | The file dialog of the system. | The file dialog of the system. | The folder dialog of the system. |
| Linux | A GTK 3 message dialog over the window of the app. | The GTK 3 file chooser, which uses the desktop portal where it runs. | The GTK 3 file chooser. | The GTK 3 file chooser. |

Browsers open their file pickers only in the moments after a click, a tap or a key press of the user, so a web app asks for files and for the save picker from an input handler, and a call without that activation fails with the code `failed`. A download needs no activation. The web closes a message that the app gives up, while a picker of the browser stays open, since pages cannot close it, and its answer goes nowhere. Windows shows its dialogs one after the other on a thread of their own, and the window of the app keeps drawing but takes no input while one shows, as with every dialog of Windows. Linux loads GTK 3 with the first dialog, and without it every dialog fails with the code `unsupported` and a message that names GTK 3. The frames of an Android app stop while a dialog has the focus, and the call settles once it closes.

## C++

`engine.getDialogs()` returns the `haylen::platform::Dialogs` of the running app, declared in `haylen/platform/Dialogs.hpp`. `show(request, callback, timeout)` validates a `haylen::platform::DialogRequest`, whose `dialog` holds a `Message`, `OpenFiles`, `SaveFile` or `OpenFolder`, throws `std::invalid_argument` with the messages above for a request the platform cannot show and returns the id of the dialog. The callback receives a `haylen::platform::DialogResult` on the frame thread, with the `button` counted from zero, the opened `files`, the `saved` file or the `folder`, or a `failure` with its `Code` and message. `cancel(id)` gives a dialog up.

```cpp
haylen::platform::DialogRequest request{.dialog = haylen::platform::DialogRequest::Message{.title = "Unsaved map", .text = "Save the changes?", .buttons = {"Save", "Discard"}}};
engine.getDialogs().show(request, [](haylen::platform::DialogResult result) {
    if (result.button == 0) {
        haylen::core::Log::info("Saving");
    }
});
```
