# 📖 `haylen.dialogs`

The module `haylen.dialogs` shows the native dialogs of the system: message boxes with up to three buttons, and pickers of files to open, of a place to save data and of a folder. They look and behave like the dialogs of every other app of the platform. Use them for desktop tools, editors and settings that import or export files. Menus and messages drawn in the style of the app belong to [`haylen.ui`](ui.md) instead.

```lua
local dialogs = require('haylen.dialogs')
```

A dialog never blocks the app. Every function shows its dialog at once and returns a [call](#calls). The choice of the user reaches Lua at the start of a later frame, where `call:await()` inside a coroutine returns it.

- A dialog that the user dismissed or cancelled returns `nil`.
- A dialog that failed returns `nil` and an [error](#errors).
- A platform that has no such dialog fails the call with the code `unsupported`, never with a default choice.

Options are tables whose unknown keys raise `Unknown option "<key>".`. Every function takes a `timeout` of its own.

## Functions

### dialogs.message(options)

Shows a message with one to three buttons. It returns a call whose `await` gives the button that the user pressed, counted from 1 in the order of `buttons`. It gives `nil` when the user dismissed the message without a button, such as with Escape or the back button.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | string | `''` | The title of the message. |
| `text` | string | required | The message. |
| `kind` | string | `'info'` | `'info'`, `'warning'` or `'error'`, which picks the icon or the style where the platform has them. |
| `buttons` | table | required | One to three labels of buttons, in the language of the app. |
| `timeout` | number | none | Seconds after which the call fails with the code `timeout`, and the platform closes the dialog. |

A missing text raises `A message dialog needs a text.`. A list of buttons that is empty or longer than three raises `A message dialog has one to three buttons, not <count>.`, and an empty label raises `Every button of a message dialog needs a label.`.

```lua
local dialogs = require('haylen.dialogs')
local scene = require('haylen.scene')

local Editor = {}

function Editor:enter()
    scene.spawn(self, function()
        local button = dialogs.message({title = 'Unsaved map', text = 'Save the changes before leaving?', kind = 'warning', buttons = {'Save', 'Discard', 'Cancel'}}):await()
        if button == 1 then
            print('Saving.')
        elseif button == 2 then
            print('Leaving without saving.')
        end
    end)
end

scene.push(Editor)
```

### dialogs.openFiles(options)

Shows a picker of files to open. It returns a call whose `await` gives a list of the picked files, or `nil` when the user cancelled. Each file is a table with `name` and `path`. The list holds one file, unless `multiple` is `true`. The table `options` is optional.

Every file can be read at its `path` with the `fs` module of Varn. On desktops the path is the file itself. On phones, tablets and the web it is a copy under `tmp/dialogs` in the folder of [`haylen.storage`](storage.md), which the engine empties whenever an app starts.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | string | `''` | The title of the picker, where the platform shows one. |
| `filters` | table | every file | A list of file types. Each is a table with a `name` and a list of `extensions` without their dot, such as `{name = 'Maps', extensions = {'tmj', 'json'}}`. |
| `multiple` | boolean | `false` | Whether the user may pick several files. |
| `timeout` | number | none | Seconds after which the call fails with the code `timeout`, and the platform closes the picker. |

These are the errors of the filters:

- A filter that is not a table with a name and a list of extensions raises `The filters of a file dialog are a list of tables with a name and a list of extensions.`.
- A filter without a name raises `Every filter of a file dialog needs a name.`.
- A filter without extensions raises `The filter "<name>" needs at least one extension.`.
- An extension with a leading dot, a wildcard, a separator or a space raises `The extension "<extension>" of the filter "<name>" is invalid. Extensions come without their dot, such as "png" or "tar.gz".`.

```lua
local async = require('async')
local dialogs = require('haylen.dialogs')
local fs = require('fs')

async.spawn(function()
    local files, err = dialogs.openFiles({title = 'Import maps', filters = {{name = 'Tiled maps', extensions = {'tmj', 'json'}}}, multiple = true}):await()
    if err then
        print('No picker here: ' .. err)
        return
    end
    for _, file in ipairs(files or {}) do
        print(file.name .. ' has ' .. #fs.readFile(file.path):await() .. ' bytes.')
    end
end)
```

### dialogs.saveFile(options)

Shows a picker of the place to save a file, and writes `data` there. It returns a call whose `await` gives the saved file, or `nil` when the user cancelled. The saved file is a table with `name` and, where the platform gives one, `path`. The web saves a download, which has no path, or a file of the file system where the browser lets the page pick one.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | string | `''` | The title of the picker, where the platform shows one. |
| `name` | string | required | The file name that the picker suggests, without folders. |
| `data` | string | required | The bytes to write, which may be empty. |
| `filters` | table | every file | File types, as `dialogs.openFiles` takes them. |
| `timeout` | number | none | Seconds after which the call fails with the code `timeout`, and the platform closes the picker. |

A missing name raises `A save dialog needs the name it suggests for the file.`, and a name with a folder raises `The name "<name>" that a save dialog suggests is a file name, without folders.`. Missing data raises `A save dialog needs the data it writes, as a string.`.

```lua
local async = require('async')
local dialogs = require('haylen.dialogs')
local json = require('json')

async.spawn(function()
    local saved = dialogs.saveFile({title = 'Export the save', name = 'island-save.json', data = json.encode({gold = 120, day = 7}), filters = {{name = 'Saves', extensions = {'json'}}}}):await()
    if saved then
        print('Exported to ' .. (saved.path or saved.name) .. '.')
    end
end)
```

### dialogs.openFolder(options)

Shows a picker of a folder. It returns a call whose `await` gives the path of the folder, or `nil` when the user cancelled. The table `options` is optional.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | string | `''` | The title of the picker, where the platform shows one. |
| `timeout` | number | none | Seconds after which the call fails with the code `timeout`, and the platform closes the picker. |

```lua
local async = require('async')
local dialogs = require('haylen.dialogs')

async.spawn(function()
    local folder, err = dialogs.openFolder({title = 'Export the level to'}):await()
    if err and err.code == 'unsupported' then
        print('This platform picks no folders, so the level goes to the storage of the app.')
    elseif folder then
        print('Exporting to ' .. folder .. '.')
    end
end)
```

## Calls

Every function returns a [`PlatformCall`](platform.md#platformcall) of `haylen.platform`, with `call:await()`, `call:cancel()` and the properties `id`, `done` and `promise`. For a dialog they work like this:

- The method `call:await()` returns the choice of the user, or `nil` and an [error](#errors).
- The method `call:cancel()` closes the dialog where the platform can, and fails the call with the code `cancelled` at the start of the next frame. It never stops the app when nothing awaits the call. It returns `true` when the dialog was still open, and `false` when it had already settled.
- The property `call.id` is the id of the dialog. It is unique in the process, so an answer that arrives after the app restarted never answers a dialog of the new app.

A timeout that is not a positive number raises `The timeout of a dialog is a positive number of seconds.`. A dialog that is still open when the app stops, such as for a hot reload, closes, and its call never settles.

```lua
local async = require('async')
local dialogs = require('haylen.dialogs')

local question = dialogs.message({text = 'Keep the new graphics settings?', buttons = {'Keep', 'Revert'}, timeout = 15})

async.spawn(function()
    local button, err = question:await()
    if button ~= 1 then
        print(err and err.code == 'timeout' and 'No answer, so the settings revert.' or 'The settings revert.')
    end
end)

local function leaveSettings()
    question:cancel()
end
```

## Errors

A failed dialog returns the same [`PlatformError`](platform.md#platformerror) as a platform call: a table with `message`, `code` and `data`, which reads as its message in `tostring` and in string concatenation.

| Code | When |
| --- | --- |
| `unsupported` | The platform has no such dialog. The message says why. |
| `cancelled` | The method `call:cancel()` gave the dialog up. |
| `timeout` | The `timeout` of the dialog passed first. |
| `failed` | The platform could not show the dialog or write the file. The message gives its reason. |

## Platforms

| Platform | Message | Open files | Save file | Open folder |
| --- | --- | --- | --- | --- |
| macOS | A sheet on the window of the app. | A sheet on the window of the app. | A sheet on the window of the app. | A sheet on the window of the app. |
| iOS, iPadOS and Mac Catalyst | An alert. | The document picker, which copies the files. | The document picker, which exports the data. | The document picker, which keeps access to the folder. |
| tvOS | An alert. | `unsupported` | `unsupported` | `unsupported` |
| Android | An `AlertDialog` of AppCompat. In the order of `buttons`, its buttons are the positive, the negative and the neutral button, so the first one sits on the right. | The document picker of the Storage Access Framework, which copies the files. It shows the types of the filters whose MIME types Android knows, or every file when an extension has none. | The document creator of the system. It never replaces a file, and it adds a number to a name that is taken. Its document has a name but no path. | `unsupported`, because Android folders have no paths. |
| Web | A dialog over the canvas in the colors of the page. Its first button takes the focus, Tab and the arrows move between the buttons, and Escape dismisses it. | The file picker of the browser, which copies the files. | The save picker of the browser where it has one, and a download elsewhere. | `unsupported`, because browsers give pages no paths of folders. |
| Windows | A task dialog that the window of the app owns. | The file dialog of the system. | The file dialog of the system. | The folder dialog of the system. |
| Linux | A GTK 3 message dialog over the window of the app. | The GTK 3 file chooser, which uses the desktop portal where it runs. | The GTK 3 file chooser. | The GTK 3 file chooser. |

These notes describe how each platform behaves:

- **Web.** Browsers open their file pickers only in the moments after a click, a tap or a key press of the user. So a web app asks for files, and for the save picker, from an input handler. A call without that activation fails with the code `failed`. A download needs no activation. The web closes a message that the app gives up. A picker of the browser stays open, since pages cannot close it, and its answer goes nowhere.
- **Windows.** Windows shows its dialogs one after the other, on a thread of their own. The window of the app keeps drawing but takes no input while one shows, as with every dialog of Windows.
- **Linux.** Linux loads GTK 3 with the first dialog. Without GTK 3 every dialog fails with the code `unsupported` and a message that names GTK 3.
- **Android messages.** An Android app keeps running while a message shows. So a cancel or a timeout closes the message at once. A message that is still open when its activity goes away closes with it.
- **Android pickers.** The pickers of Android belong to another app, whose activity pauses the app and its frames. So a cancel or a timeout of a picker takes effect once the picker closed. The app cannot close a picker, and one shows at a time, so a second one fails with the code `failed`. Devices without a document picker, as Android TV devices often are, fail the pickers with the code `unsupported`. A picker that answers after its activity or its process ended, which the stable keys of the Activity Result API allow, finds no call waiting, and its answer is logged and dropped.
- **Apple alerts.** The alerts of iOS, iPadOS, Mac Catalyst and tvOS close only through their buttons, so a message there never answers `nil` unless the app gives it up. The alert of macOS takes Return for its first button, and Escape for a button named Cancel.
- **Apple files.** On iOS, iPadOS and Mac Catalyst the path of a saved file is the place that the person picked, which the app cannot read later. A picked folder stays readable until the app ends.

## C++

The function `engine.getDialogs()` returns the `haylen::platform::Dialogs` of the running app, declared in `haylen/platform/Dialogs.hpp`.

| Member | Meaning |
| --- | --- |
| `show(request, callback, timeout)` | Validates a `haylen::platform::DialogRequest`, whose `dialog` holds a `Message`, an `OpenFiles`, a `SaveFile` or an `OpenFolder`, and returns the id of the dialog. It throws `std::invalid_argument`, with the messages above, for a request that the platform cannot show. The callback receives a `haylen::platform::DialogResult` on the frame thread. |
| `cancel(id)` | Gives a dialog up. |

The `DialogResult` holds the `button` counted from zero, the opened `files`, the `saved` file or the `folder`, or a `failure` with its `Code` and message.

```cpp
haylen::platform::DialogRequest request{.dialog = haylen::platform::DialogRequest::Message{.title = "Unsaved map", .text = "Save the changes?", .buttons = {"Save", "Discard"}}};
engine.getDialogs().show(request, [](haylen::platform::DialogResult result) {
    if (result.button == 0) {
        haylen::core::Log::info("Saving");
    }
});
```
