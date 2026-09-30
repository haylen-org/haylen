# Haylen Filesystem

A Lua sample with one scene per way an app reads and writes files: the files of its own package through [`haylen.assets`](../../../docs/lua-api/assets.md), the private user folder through [`haylen.storage`](../../../docs/lua-api/storage.md) and through Varn's `fs` module, save slots, and zip archives through Varn's `zip` module. The menu lists the tests, each test opens as its own scene with a Back button, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu. The [Lua guide](../../../docs/lua.md#disk-access) compares the two ways to reach the user folder.

| Test | What it shows |
| --- | --- |
| Package files | The function `assets.list` over `content/data`, and `assets.text`, `assets.json` and `assets.bytes` reading a note, a JSON document and a small binary level format parsed with `string.unpack`, plus the error a missing file raises. |
| User folder with storage | Synchronous `storage.writeText`, `storage.readText`, appending by reading and writing back, `storage.writeJson` and `storage.readJson`, `storage.exists` for files and folders, `storage.list` with the size of every file, `storage.remove`, `storage.flush`, and the errors of a missing file and of a path that leaves the folder. |
| User folder with fs | Asynchronous `fs.mkdir`, `fs.writeFile`, `fs.append`, `fs.readFile`, `fs.stat`, `fs.readdir`, `fs.copy`, `fs.rename`, a 2 MB file streamed through `fs.open` handles with a progress bar, and `fs.removeRecursive`, all over `storage.root()` and awaited in tasks the scene owns. |
| Save slots | The functions `storage.writeSlot`, `storage.readSlot`, `storage.slotInfo`, `storage.listSlots` and `storage.removeSlot` behind a small save and load screen with a confirmation dialog, an autosave written when the test closes, and a damaged slot file reported instead of crashing. |
| Zip archives | The functions `zip.create` over files of the user folder, `zip.list` and `zip.extract`, and an archive shipped in `content/archives` that is copied to the user folder and unpacked. |
| File browser | Every folder and file of the user folder with its size and date from `fs.stat`, a preview of the first bytes read through a handle, and buttons that add examples, folders and files and delete files. |

Everything the tests write stays inside the folder the engine gives the app, named after the identifier `dev.haylen.samples.filesystem`, so running the sample never touches other files. The file browser shows that folder, including the files the other tests leave behind.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run samples/system/filesystem` |
| macOS app | `python3 make.py run samples/system/filesystem --platform macos` |
| iPhone and iPad simulator | `python3 make.py run samples/system/filesystem --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run samples/system/filesystem --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run samples/system/filesystem --platform android --device <serial>` |
| Browser | `python3 make.py run samples/system/filesystem --platform web` |

In the browser the user folder lives in the storage of the page, which the engine keeps in IndexedDB, so files survive a reload of the page.

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test, a button or a row | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
| Scroll a list or a preview | Mouse wheel | Moving the focus | Drag | Moving the focus |
