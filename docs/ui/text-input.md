# ⌨️ Text input

This guide explains how the text fields of the UI type through the native text input of each platform, and what each platform does. It is for developers who want to know why typing works the way it does, and for anyone who ports or debugs a platform host. The [UI guide](ui.md#text-entry) shows how an app uses text fields, and the [`haylen.ui` reference](../lua-api/ui.md#uitextfieldproperties) lists their keyboard options.

A text field of the UI does not read keys on its own. A hidden native field of the platform edits its text. So phones, tablets and TVs open their on-screen keyboard. Every platform but Linux brings its input methods for languages such as Japanese and Chinese. Those platforms also bring their autocorrection, dictation, emoji picker and native paste, copy and undo. The UI still draws the field, its text, its selection and its caret.

## How a field edits

When a text component of [`haylen.ui`](../lua-api/ui.md) takes the focus, the UI publishes the field to the platform. It sends these values:

- An id and a revision, a number that counts the changes the UI made.
- The text in UTF-8, and the selection in code points.
- The rectangles of the field and of its caret, in framebuffer pixels.
- The keyboard: text, multiline, number, decimal, phone, email, url, search or password.
- The label of the return key: default, done, go, next, search or send.
- The autocorrection, the capitalization and the length limit.

The platform places its native field over the field of the UI and shows the keyboard.

What the user types comes back as the whole text of the native field, its selection and the range that the input method composes. It comes with the revision that the native field last received. The UI applies it inside the ImGui callback of the field, on the frame thread, which keeps the undo history and the caret of the field right.

The revision grows only when the UI itself changes the field. That happens when a click moves the caret, when the length limit cuts the text, or when the app sets a new value. An edit made against an older revision answers a text that the field no longer shows. So the UI drops it, and the platform receives the new text instead. While the input method composes, the UI publishes nothing, so it never interrupts the composition. The UI underlines the composing text in the field.

Return, tab and escape of the native field arrive as actions:

- Submit reports `submit` and lets the field go. A return key labelled next moves on to the next field, like tab.
- Next moves the focus to the next field, like tab.
- Cancel brings back the text that the field had when it took the focus, and lets the field go.
- A keyboard that the user closed lets the field go and keeps its text.

While a field edits, the keys it types belong to the field alone and never navigate. So Space types a space, and Enter starts a new line in a text area instead of pressing the field. Enter still submits a single line and Escape still cancels, as keys of the field. The gamepad buttons of `uiAccept` and `uiCancel` still end the editing. While a platform edits the field natively, the UI ignores the character events and editing keys that reach it for that field, so nothing types twice.

The platform also reports the frame of the on-screen keyboard. When the keyboard would cover the focused field, every mounted GUI moves up together until the field shows above it, with a small margin. They never move past the top of the screen, and they move back when the keyboard closes. The engine publishes `keyboardShown`, with the frame in design units, and `keyboardHidden` on the [event bus](../lua-api/events.md).

### Typing without a field of the UI

The function `window.setKeyboardVisible(true)` of [`haylen.window`](../lua-api/window.md) opens the same native field without a text field of the UI. It is for apps that read typing themselves. The runtime turns the edits into the `character` events, and the `backspace`, `enter`, `tab` and `escape` key events, that a physical keyboard would send. A scene reads them in its [`event` callback](../lua-api/input.md). Text inside an open composition types once the input method commits it. The text inputs of [`haylen.imgui`](../lua-api/imgui.md) use this plain keyboard too.

```lua
local scene = require('haylen.scene')
local window = require('haylen.window')

scene.push({
    typed = '',
    enter = function(self)
        window.setKeyboardVisible(true)
    end,
    event = function(self, e)
        if e.type == 'character' then
            self.typed = self.typed .. e.character
        end
    end,
})
```

## Web

The web runtime, `engine/platform/web/haylen-runtime.js`, edits through fields of the browser:

- A hidden `<textarea>` edits text, text areas and every keyboard except passwords. A hidden `<input type="password">` edits passwords.
- Both are siblings of the canvas, placed over the field, with framebuffer pixels scaled to the box of the canvas on the page.
- They have opacity 0, a transparent caret and a font of 16 pixels, which keeps Safari on iOS from zooming into the page.
- The element gets `inputmode`, `enterkeyhint`, `autocapitalize`, `autocorrect`, `spellcheck` and `maxlength` from the options of the field.

The element reports its text on `input`, its composition on `compositionstart`, `compositionupdate` and `compositionend`, and its selection on `selectionchange`. Enter of a single line, Tab and Escape become the submit, next and cancel actions. Paste, copy, cut and undo stay native.

The element stops the propagation of its key events, so the listeners of `sokol_app` on the window never apply a key twice. Any other element of the page that has the focus keeps its keys the same way. Examples are a field of an editor next to the canvas and a button of a plugin. The focus of elements never reaches `sokol_app`, which hears only the focus of the window. A key release still reaches `sokol_app` whenever its press did, even when an element took the focus in between, such as the field that a held Tab started editing. So no key stays held. The page forgets the pressed keys when the window loses the focus, where the engine releases them.

A `blur` while the page keeps the focus means that the user left the field or closed the keyboard, which sends the dismissed action. The events `resize` and `scroll` of the `visualViewport` report the part of the canvas that the keyboard covers.

Safari on iOS opens the keyboard only when an element takes the focus inside a user gesture, but the engine learns about a tap only on its next frame. So the engine publishes the rectangles and options of every text field on screen. When a tap lands on one, the page focuses the right element at once, in `pointerup`. Desktop browsers take the same path, which gives them input methods as well. The plain keyboard of `window.setKeyboardVisible` leaves key events to `sokol_app` and takes only the text. On iOS, a page opens it only from such a tap, so a text field of the UI is the reliable way to type there.

## Android

`HaylenActivity` adds a hidden `HaylenEditText` with alpha 0 to the layout of the surface of the app, above the surface. The engine places it over the focused field, in window pixels. It is an ordinary view of the activity, which GameActivity hosts, so it takes the focus and the keyboard like any field. Touches pass through it to the app, which places the caret itself.

- Its `inputType` follows the keyboard, the capitalization and the autocorrection of the field.
- Its `imeOptions` carry the action of the return key, with `IME_FLAG_NO_EXTRACT_UI` and `IME_FLAG_NO_FULLSCREEN`. So the input method never covers the app with its own editor.
- Every call from the engine runs on the UI thread through `runOnUiThread`.
- After `afterTextChanged` and `onSelectionChanged`, once the input method finishes its batch of changes, the field sends its text, selection and composing span through JNI. The method `onEditorAction` sends the submit action.

The field keeps the editing of Android: the composition of input methods, suggestions and autocorrection, the clipboard, and the length limit through `InputFilter.LengthFilter`. The input method places its candidates next to the field of the UI, since the hidden field lies over it. GameActivity also offers GameTextInput, an input connection on the surface of the app, which the engine leaves unused. It knows neither where the field is nor its length limit, and it leaves the keys of hardware keyboards out of the text.

While the field edits, it has the focus. So the keys of hardware keyboards type into it, and tab, escape and return become actions. The keys that it leaves reach the app, and touches and the sticks of controllers always reach the app. GameActivity focuses its surface whenever its window gains the focus, such as after a dialog or the notification shade. The surface declines the focus while a text field over the app has it, so the field keeps its keyboard.

The activity uses `adjustNothing`, so the keyboard lies over the app without resizing it. Its insets listener reports the insets of the input method as the keyboard frame. A keyboard that closes while a field edits, such as with the back button, sends the dismissed action. Back never leaves the app while a field edits. The keyboard opens only when a field starts editing, so a field that moves or changes never brings back a keyboard that the user closed. Android 11 and later report the insets of the input method for a window that draws edge to edge. Android 8.1 to 10 may not report them with `adjustNothing`, and there the UI does not move above the keyboard.

## iOS, iPadOS, tvOS and Mac Catalyst

The runtime adds a hidden `UITextField` for single lines and a hidden `UITextView` for text areas to the root view. It places them over the field in points, with alpha 0, so touches still reach the view of the app.

- They get `keyboardType`, `returnKeyType`, `secureTextEntry`, `autocorrectionType` and `autocapitalizationType` from the field, without smart quotes and dashes.
- The editing changed event and `textFieldDidChangeSelection`, or their equivalents of the text view, send the text, the selection and the `markedTextRange` of the input method. The runtime converts them from UTF-16 offsets to code points.
- Return sends the submit action. Tab and escape of a hardware keyboard send next and cancel. A field that stops editing on its own sends the dismissed action.
- `UIKeyboardWillChangeFrameNotification` reports the keyboard frame. The module `sokol_app` no longer shows its own keyboard.

On tvOS, the text field presents the fullscreen keyboard of the system, and a text area edits in a text field too. Mac Catalyst uses the same path as iOS, with the keyboard of the Mac.

## macOS

While a field edits, an invisible `NSTextView` over it is the first responder of the window. It brings marked text, dead keys, the emoji picker and dictation.

- The Edit menu of the app gives it Undo, Redo, Cut, Copy, Paste, Delete and Select All, with their Command shortcuts. The text system of AppKit adds the moves by word, line and document with Option and Command.
- The selector `insertNewline:` submits a single line, `insertTab:` moves on, and `cancelOperation:` cancels.
- Clicks pass through it to the view of the app, which places the caret itself. When the editing ends, the view of the app becomes the first responder again.
- Password fields accept only Roman input sources, like the secure text fields of the system.

## Windows and Linux

Neither platform has a native field, so the UI edits the text itself:

- Windows types through `WM_CHAR`, which `sokol_app` turns into character events. The input method composes in its own window. The text input places the composition window at the caret of the focused field with `ImmSetCompositionWindow`, and the candidate list below the caret line with `ImmSetCandidateWindow`. So the input method never covers the text being typed.
- Linux types through the key and character events of X11, which `sokol_app` reads without an input method of X11. So text that an input method composes, such as Japanese, Chinese or Korean, does not reach Linux apps, while the characters of the keyboard layout do.
- The UI edits with Ctrl+A, Ctrl+C, Ctrl+X, Ctrl+V, Ctrl+Z, Ctrl+Y and Ctrl+Shift+Z. It moves by word with Ctrl and the arrows, and to the ends of the line with Home and End.

## Complex scripts and right-to-left text

The UI draws the text of a field with the shaped layout of the engine. So a field shows joined Arabic letters, Indic conjuncts, the marks of every script and the bidirectional order of mixed text, as a label does. A password shows one bullet per character. ImGui still keeps the text, the undo history and the editing keys, but its own glyphs know no shaping or direction. So the field places the caret again by the shaped text.

The caret stops only between characters, never inside a letter with its marks, a conjunct or a ligature. It stands on the side of the character it follows, which is the right side of a right-to-left letter. A selection across a change of direction covers every run it spans, which may lie apart on screen.

On every platform, a press puts the caret at the character under the pointer. A double press selects the word there, and a drag selects up to the pointer. Where the UI edits the text itself, on Windows and Linux, the left and right arrow keys move the caret one character the way they point on screen. The up and down arrow keys of a text area move it to the nearest character of the line above or below. Where a native field edits, the native field moves the caret with the keys, by the rules of the platform. The UI then draws the caret that the field reports.

A field lines its text up with the side where the direction of the UI starts, which is the right of a right-to-left interface. Each paragraph of the text reads in the direction of its first strong letter. A single line longer than the field scrolls toward the side it grows to. A text area wraps its long lines at its width and scrolls only up and down.

Some limits remain:

- On Windows and Linux, word jumps with a modifier key follow the reading order of the text rather than the order on screen. Home and End go to the start and the end of the line in reading order.
- Backspace deletes one code point, which removes the last mark of a letter before the letter itself.
- A caret at the boundary between a left-to-right and a right-to-left run has two places on screen. It stands on one of them: the side of the character it follows.

## What else the hosts follow

The hosts that edit text also follow other parts of the platform:

- On the web, a hidden tab, through `visibilitychange`, sends the app to the background, and a visible one brings it back. The event `pagehide` starts saving the user data once more, which the browser finishes when it lets the page go. Browsers keep audio suspended until the user interacts, and Safari on iOS suspends it again after interruptions. So every `pointerdown`, `touchend` and `keydown` resumes the audio contexts of miniaudio that the app keeps playing, unless the app is in the background or the page paused it. The events `online` and `offline` report the network. The Screen Orientation API reads and locks the orientation where the browser allows it, which usually means a fullscreen page on a phone.
- On Android, the activity holds the audio focus in the foreground and abandons it in the background. Another app that takes it, for a phone call or an alarm, interrupts the app. The engine then makes the app inactive, and the audio plugin pauses every voice until the focus comes back. A loss that lets the app play more quietly needs nothing, since the system lowers the volume itself. `HaylenNetwork` follows the default network with a `ConnectivityManager` callback. The function `window.lockOrientation` calls `setRequestedOrientation`. The function `window.setStatusBar` shows or hides the status bars of `WindowInsetsControllerCompat` and sets their light or dark look. The activity reports the height of the status bar with the safe area.
- On iPhone and iPad, `window.lockOrientation` asks UIKit for the new orientations through the application delegate, and turns the screen with `requestGeometryUpdate`. The function `window.setStatusBar` asks UIKit to update the status bar, which `HaylenViewController`, the view controller of the window, answers with `prefersStatusBarHidden` and `preferredStatusBarStyle`. `NWPathMonitor` reports the network on every Apple platform.

## From C++

The class `haylen::platform::TextInput` (`haylen/platform/TextInput.hpp`) is the protocol, and `Window::getTextInput()` returns the text input of the platform.

- The method `edit(field)` starts or follows the editing of a `TextInput::Field`, `finish()` ends it, and `setVisibleFields(fields)` lists the fields on screen.
- A native platform answers with `TextEdited` events, which carry a `TextInput::Edit`, `TextAction` events with a `TextInput::Action`, and `KeyboardChanged` events with the keyboard frame. The engine delivers them to the UI on the frame thread.
- A text input that keeps the defaults of the class, as on Linux, leaves the typing to key and character events.
- The method `ui::Backend::getKeyboardOffset()` returns how far the UI moves up above the keyboard. The methods `getLiftedDisplayRect()` and `getLiftedSafeRect()` return the display and safe areas moved up by it, where the GUIs place everything they draw.
