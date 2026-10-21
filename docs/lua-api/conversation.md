# 📖 `haylen.conversation`

The module `haylen.conversation` runs branching conversations: nodes of lines with a speaker and tags, choices that conditions allow, variables, jumps, commands that reach the app, visit counts, variants that a seeded generator picks, and saves that resume a conversation where it stood. Use it for dialogue, tutorials, onboarding and any script of lines and answers. The writing lives in `.conversation` files of the app, and the runtime only walks them.

```lua
local conversation = require('haylen.conversation')
```

Conversations load as assets of the type `conversation` from `.conversation` files, with `assets.load` or `assets.loadAsync`, which read them on a worker thread, or from a table with `conversation.new`.

## Files

A `.conversation` file is a JSON object with `variables`, their values before the conversation sets them, and `nodes`, the nodes by name. A node has `steps`, which run in order, then `choices`, or `next`, the node that follows when it offers no choice. A node with neither ends the conversation.

```json
{
    "variables": {"gold": 30, "metSmith": false},
    "nodes": {
        "greeting": {
            "steps": [
                {"speaker": "smith", "text": "smith.hello", "tags": ["happy"], "if": "not metSmith"},
                {"speaker": "smith", "text": "smith.again", "if": "metSmith"},
                {"command": "wave", "arguments": {"hand": "left"}},
                {"set": {"metSmith": true}}
            ],
            "choices": [
                {"text": "choice.buy", "if": "gold >= 10", "next": "buy"},
                {"text": "choice.rumor", "next": "rumor", "once": true},
                {"text": "choice.leave"}
            ]
        },
        "buy": {"steps": [{"set": {"gold": "gold - 10"}}, {"command": "giveItem", "arguments": {"item": "sword"}}, {"text": "smith.thanks"}], "next": "greeting"},
        "rumor": {"steps": [{"text": ["rumor.mine", "rumor.road", "rumor.storm"]}], "next": "greeting"}
    }
}
```

| Step | Keys | Meaning |
| --- | --- | --- |
| Line | `text`, `speaker`, `tags` | Shows a line and waits for `state:advance()`. A list of texts is a variant, from which the generator of the state picks one. |
| Command | `command`, `arguments` | Hands the name and the arguments to the listeners of `command`, such as to give an item or play a sound. |
| Set | `set` | Sets variables, each to an expression, a number or a boolean. A text of a string is written in quotes inside the expression, such as `"'Brave'"`. |
| Jump | `jump` | Goes on in another node. |

Every step and every choice may have `if`, an expression that must be true for the step to run or the choice to show. A choice has `text`, `next`, the node it leads to, or none to end the conversation, and `once`, which removes it after it was taken. Text is a key of the [localization](localization.md) catalog, or the text itself when the catalog has no such key, so a conversation works before it is translated, and either one fills its `{name}` placeholders with the variables. The texts of a conversation, which translators turn into keys, come from `conversation.texts`.

Expressions use numbers, strings in single or double quotes, `true`, `false`, `nil`, variables by name, `visits.<node>` for how often a node was entered, `+`, `-`, `*`, `/`, `==`, `~=`, `<`, `<=`, `>`, `>=`, `and`, `or`, `not` and parentheses, with the truth of Lua, where only `false` and `nil` are false. A variable that was never set is `nil`.

A mistake in a file names its place, such as `The step 2 of the node "greeting" leads to the node "forge", which the conversation does not have.`, `The choice 1 of the node "greeting" has a condition it cannot read. The expression "gold >" ends where a value belongs at character 7.` or `The node "buy" has the unknown key "colour".`.

## Functions

### conversation.new(document)

Reads a conversation from a table in the format of the files.

```lua
local conversation = require('haylen.conversation')

local talk = conversation.new({nodes = {start = {steps = {{text = 'Hello.'}}}}})
```

## Conversation

| Member | Meaning |
| --- | --- |
| `talk:start(node, options)` | Starts a walk at a node and returns a `ConversationState` that shows its first line, its choices or has finished. A missing node raises `The conversation has no node "<node>".`. |
| `talk:restore(saved)` | Returns a state that resumes from what `state:save()` returned. A save that does not fit raises `The saved conversation does not fit this conversation: <reason>.`. |
| `talk.texts` | The texts of every line and choice once, in order. |
| `talk.nodes` | The names of the nodes. |

| Option of `start` | Type | Default | Meaning |
| --- | --- | --- | --- |
| `variables` | table | `{}` | Values over the variables of the file. |
| `seed` | integer | random | The seed of the generator that picks variants, so the same seed picks the same variants. |

```lua
local assets = require('haylen.assets')

local talk = assets.load('dialogue/smith.conversation')
local state = talk:start('greeting', {variables = {gold = 12}, seed = 7})
print(state.line.text)
```

## ConversationState

| Member | Meaning |
| --- | --- |
| `state.line` | The line it shows as `{speaker, text, key, tags}`, where `text` is the localized text and `key` the text of the file, or `nil`. The last line stays while the choices that follow it show, so a question shows with its answers. |
| `state.choices` | The choices that their conditions allow as `{text, key}`, empty while none waits. |
| `state.finished` | Whether the conversation ended. |
| `state.variables` | A copy of the variables. |
| `state.node` | The name of the node it stands in. |
| `state:advance()` | Goes on from the line. It raises `The conversation waits for a choice. Call "choose" with one of its choices.` while choices wait and `The conversation has finished. Start it again to go on.` once it ended. |
| `state:choose(index)` | Takes a choice of `state.choices`, counted from 1. Without choices it raises `The conversation offers no choices now.`, and outside them `The conversation offers 2 choices, so there is no choice 3.`. |
| `state:setVariable(name, value)` | Sets a variable, such as one the app changed outside the conversation. |
| `state:visits(node)` | How often the walk entered the node. |
| `state:save()` | Returns a table of the whole state, which `storage.writeSlot` keeps and `talk:restore` resumes. |
| `state:on('command', fn, options)` | Calls `fn(name, arguments)` for every command the walk runs, after the call that ran it returns, and returns the [`Connection`](signal.md#connection). The option `owner` ends the listener with its owner. Commands that ran before the first listener reach it at once. |

Steps that run in a row without a line, such as jumps that lead in a circle, stop after 10000 with `The conversation ran 10000 steps without a line in the node "<node>", which looks like a loop of jumps.`.

```lua
local assets = require('haylen.assets')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    enter = function(self)
        self.talk = assets.load('dialogue/smith.conversation'):start('greeting', {variables = {gold = 30}})
        self.talk:on('command', function(name, arguments)
            if name == 'giveItem' then
                print('Received ' .. arguments.item .. '.')
            end
        end, {owner = self})
        self:show()
    end,
    show = function(self)
        if self.gui then
            self.gui:unmount()
        end
        local rows = {ui.label{text = self.talk.line and self.talk.line.text or ''}}
        for index, choice in ipairs(self.talk.choices) do
            rows[#rows + 1] = ui.button{text = choice.text, onClick = function()
                self.talk:choose(index)
                self:show()
            end}
        end
        if #self.talk.choices == 0 and not self.talk.finished then
            rows[#rows + 1] = ui.button{text = 'Continue', onClick = function()
                self.talk:advance()
                self:show()
            end}
        end
        self.gui = ui.mount(ui.column(rows), {owner = self})
    end,
})
```
