# 📖 `haylen.conversation`

The module `haylen.conversation` runs branching conversations, such as dialogue with characters, tutorials and onboarding. This page is for developers who write those conversations and show them on screen. A conversation is a set of named nodes. A node shows lines with a speaker and tags, runs commands of the app, sets variables, and offers choices that lead to other nodes. The writing lives in `.conversation` files of the app. The module walks them one line or choice at a time, picks variants with a seeded generator, counts visits and saves a walk so that it resumes where it stood.

```lua
local conversation = require('haylen.conversation')
```

## Loading

A `.conversation` file loads as an asset of the type `conversation`, which is a [`Conversation`](#conversation). The function [`assets.load`](assets.md#assetsloadpath-type-options) reads the file at once. The function [`assets.loadAsync`](assets.md#assetsloadasyncpath-type-options) reads and checks it on a worker thread. The asset takes no options, so any option raises `Unknown option "<key>".`. A Lua table in the same format becomes a conversation with [`conversation.new`](#conversationnewdocument).

To run a conversation, start a walk with [`talk:start`](#talkstartnode-options). It returns a [`ConversationState`](#conversationstate), which shows a line, offers choices or has finished. Show `state.line` and `state.choices`, then call `state:advance()` after a line or `state:choose(index)` when choices wait.

```lua
local assets = require('haylen.assets')

local talk = assets.load('dialogue/smith.conversation')
local state = talk:start('greeting')
print(state.line.speaker .. ': ' .. state.line.text)
```

```lua
local async = require('async')
local assets = require('haylen.assets')

async.spawn(function()
    local talk, failure = assets.loadAsync('dialogue/smith.conversation'):await()
    if talk then
        print('The conversation has ' .. #talk.nodes .. ' nodes.')
    else
        print(failure)
    end
end)
```

## Files

A `.conversation` file is a JSON object with two keys. The object `variables` is optional and gives each variable its value before the conversation sets it. The object `nodes` holds at least one node by name. The examples of this page use this file, saved as `content/dialogue/smith.conversation`.

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

### Nodes

A node is an object with three optional keys.

| Key | Type | Meaning |
| --- | --- | --- |
| `steps` | list | The steps that run in order when the walk enters the node. |
| `choices` | list | The choices that the node offers after its steps. |
| `next` | string | The node that follows when the node offers no choice. A `null` is the same as leaving it out. |

A walk goes through a node like this:

1. The walk enters the node, and the visit count of the node goes up by one.
2. The steps run in order. A step whose `if` is false is skipped.
3. A line stops the walk until `state:advance()` is called.
4. After the last step, the node offers the choices that their `if` allows and that are not used up. The walk waits for `state:choose`.
5. When no choice is offered, the walk goes on to the node `next`. A node without `next` ends the conversation.

### Steps

Each step is an object of one kind. The first of the keys `text`, `command`, `set` and `jump` that the step holds decides its kind, in that order. Every step may also have `if`, an [expression](#expressions) that must be true for the step to run. A key that the kind does not take is an error, so one step cannot both show a line and run a command.

| Kind | Keys | What it does |
| --- | --- | --- |
| Line | `text`, `speaker`, `tags`, `if` | Shows a line and waits for `state:advance()`. |
| Command | `command`, `arguments`, `if` | Hands the name and the arguments to the listeners of the [`command`](#events) event, such as to give an item or play a sound. |
| Set | `set`, `if` | Sets variables, each to the value of an expression. |
| Jump | `jump`, `if` | Goes on at once at the first step of another node. The steps after the jump do not run. |

The keys of each kind:

- The `text` of a line is one text or a list of texts. A list is a variant: each time the step runs, the generator of the walk picks one of its texts.
- The `speaker` of a line is a text, and its `tags` are a list of texts, such as moods for a portrait.
- The `command` is a name that is not empty. The `arguments` are any JSON value, and an empty object when left out.
- The `set` is an object that maps each variable to an expression, a number or a boolean. A string value is an expression, so a text goes in quotes inside it, such as `"'Brave'"`. The variables of one step are set one after another, sorted by name. So an expression sees the new values of the names that come before it in that order.
- The `jump` is the name of a node. Entering it counts as a visit.

```json
{
    "nodes": {
        "gate": {
            "steps": [
                {"speaker": "guard", "text": "Who goes there?", "tags": ["stern"]},
                {"set": {"title": "'Brave'", "warned": true, "visitsLeft": "visitsLeft - 1"}},
                {"command": "playSound", "arguments": {"path": "audio/horn.ogg"}, "if": "visitsLeft <= 0"},
                {"jump": "closed", "if": "visitsLeft <= 0"},
                {"text": ["Pass.", "Go on.", "Move along."]}
            ]
        },
        "closed": {"steps": [{"text": "The gate is closed."}]}
    },
    "variables": {"visitsLeft": 3}
}
```

### Choices

A choice is an object with these keys.

| Key | Type | Meaning |
| --- | --- | --- |
| `text` | string | The text of the choice. It is required. |
| `next` | string | The node that the choice leads to. Without it, or with `null`, the choice ends the conversation. |
| `once` | boolean | With `true`, the choice disappears for the rest of the walk once it was taken. The default is `false`. |
| `if` | string | An [expression](#expressions) that must be true for the choice to show. |

```json
{"text": "choice.rumor", "next": "rumor", "once": true, "if": "visits.greeting > 1"}
```

### Texts

The text of a line or a choice is a key of the [localization](localization.md) catalog, or the text itself when the catalog has no such key. So a conversation works before it is translated. Both fill their `{name}` placeholders with the variables of the walk, as [`localization.text`](localization.md#localizationtextkey-arguments) fills them with its arguments. The marks `{{` and `}}` give single braces, and a placeholder without a variable stays as it is. Since the variables are the arguments, a variable named `count` also picks the plural form of a text of the catalog. The property [`talk.texts`](#talktexts) lists every text once, which translators turn into keys.

```json
{"steps": [{"speaker": "smith", "text": "You have {gold} coins, {name}."}]}
```

### Expressions

The `if` of a step or a choice and the values of `set` are expressions. They read like Lua expressions.

| Part | Examples and rules |
| --- | --- |
| Numbers | `10`, `2.5`, `1e3`. A number starts with a digit, and a whole number stays an integer. |
| Strings | `'Brave'` or `"Brave"`. A string ends at the next quote of its kind and has no escapes. |
| Values | `true`, `false` and `nil`. |
| Variables | `gold`, `metSmith`. A name holds letters, digits, `_` and `.`, and does not start with a digit. A variable that was never set is `nil`. |
| Visits | `visits.greeting` is how often the walk entered the node `greeting`, and `0` for a node that it never entered. A node whose name holds other characters, such as `-`, cannot be named this way. |
| Arithmetic | `+`, `-`, `*`, `/` and the minus sign, as in `-gold`. |
| Comparisons | `==`, `~=` or its other spelling `!=`, `<`, `<=`, `>` and `>=`. |
| Logic | `and`, `or` and `not`. |
| Grouping | `(` and `)`. |

These rules decide the value of an expression:

- Truth is the truth of Lua: only `false` and `nil` are false, so `0` and `''` are true.
- The words `and` and `or` return one of their values, as in Lua. So `title or 'Traveler'` gives a default.
- From the loosest to the tightest, the order is `or`, `and`, `not`, the comparisons, `+` and `-`, `*` and `/`, and the minus sign. Unlike Lua, `not` is looser than a comparison, so `not gold > 5` means `not (gold > 5)`.
- An expression holds one comparison without parentheses. Write `gold > 5 and gold < 20` instead of a chain.
- Integers stay integers through `+`, `-` and `*`, and `/` always gives a decimal number.
- The operators `==` and `~=` compare numbers by value, so `1 == 1.0` is true, and other values by their type and content.
- Arithmetic and the comparisons `<`, `<=`, `>` and `>=` need numbers. Any other value raises `The expression "<text>" uses "<operator>" with a value that is not a number.` when the step runs or the choice is offered. The error comes from the call that ran it, which is `talk:start`, `state:advance` or `state:choose`.

```json
{"text": "choice.discount", "if": "(gold >= 10 or title == 'Brave') and not visits.buy > 2"}
```

### File errors

A mistake in a file names its place, and the load or `conversation.new` raises it. In this table, `<place>` is `The node "<node>"`, `The step <number> of the node "<node>"` or `The choice <number> of the node "<node>"`, with steps and choices counted from 1. A file that is not valid JSON fails with the message of the JSON reader, which names the line and the column of the mistake.

| Message | Cause |
| --- | --- |
| `A conversation is an object with "nodes", an object of at least one node by name.` | The document is not an object, or its `nodes` is missing, not an object or empty. |
| `The conversation has the unknown key "<key>".` | The document holds a key other than `variables` and `nodes`. |
| `The "variables" of a conversation are an object of values by name.` | The `variables` are not an object. |
| `The node "<node>" is not an object with "steps", "choices" and "next".` | A node is not an object. |
| `The node "<node>" has "steps" that are not a list.` | The `steps` of a node are not a list. |
| `The node "<node>" has "choices" that are not a list.` | The `choices` of a node are not a list. |
| `<place> has the unknown key "<key>".` | A node, a step or a choice holds a key that it does not take, such as `colour`. |
| `<place> leads to something other than the name of a node.` | A `next` or a `jump` is not a text. |
| `<place> leads to the node "<target>", which the conversation does not have.` | A `next` or a `jump` names a node that does not exist. |
| `The step <number> of the node "<node>" is not an object.` | A step is not an object. |
| `The step <number> of the node "<node>" has none of "text", "command", "set" and "jump".` | A step has no kind. |
| `The step <number> of the node "<node>" has a "text" that is not a text or a list of texts to pick from.` | The `text` of a line is not a text or a list of texts that is not empty. |
| `The step <number> of the node "<node>" has a "speaker" that is not a text.` | The `speaker` of a line is not a text. |
| `The step <number> of the node "<node>" has "tags" that are not a list of texts.` | The `tags` of a line are not a list of texts. |
| `The step <number> of the node "<node>" has a "command" that is not a name.` | The `command` is empty or not a text. |
| `The step <number> of the node "<node>" has a "set" that is not an object of values by variable.` | The `set` is not an object. |
| `The step <number> of the node "<node>" sets "<variable>" to something other than an expression, a number or a boolean.` | A value of `set` is `null`, a list or an object. |
| `The step <number> of the node "<node>" sets "<variable>" badly. <expression error>` | An expression of `set` cannot be read. |
| `<place> has an "if" that is not an expression.` | The `if` of a step or a choice is not a text. |
| `<place> has a condition it cannot read. <expression error>` | The `if` of a step or a choice cannot be read. |
| `The choice <number> of the node "<node>" is not an object with a "text".` | A choice is not an object or has no `text`. |
| `The choice <number> of the node "<node>" has a "once" that is not a boolean.` | The `once` of a choice is not `true` or `false`. |

An expression that cannot be read gives one of these errors, which the messages above carry, with characters counted from 1.

| Expression error | Cause |
| --- | --- |
| `The expression "<text>" ends where a value belongs at character <number>.` | The text ends after an operator, such as `gold >`. |
| `The expression "<text>" misses a closing parenthesis at character <number>.` | A `(` has no `)`. |
| `The expression "<text>" has a string that is not closed at character <number>.` | A quote has no closing quote. |
| `The expression "<text>" has a number it cannot read at character <number>.` | A number is broken. |
| `The expression "<text>" has "<word>" where a value belongs at character <number>.` | A word or a sign stands where a value belongs, such as `gold > and`. |
| `The expression "<text>" has text after its end.` | Something follows a whole expression, such as `gold 5` or a second comparison. |

```lua
local conversation = require('haylen.conversation')

local ok, message = pcall(conversation.new, {nodes = {greeting = {choices = {{text = 'Buy.', ['if'] = 'gold >'}}}}})
print(message) -- The choice 1 of the node "greeting" has a condition it cannot read. The expression "gold >" ends where a value belongs at character 7.
```

## Functions

### conversation.new(document)

Reads a conversation from the Lua table `document`, in the format of the [files](#files), and returns a [`Conversation`](#conversation). The table becomes JSON first. A table whose keys run from 1 without gaps becomes a list, and any other table becomes an object. An empty table becomes an object, so leave out an empty `steps`, `choices` or `tags` instead of writing `{}`. A value that JSON cannot hold, such as a function, raises `A <type> cannot be converted to JSON.`. A mistake in the format raises one of the [file errors](#file-errors).

```lua
local conversation = require('haylen.conversation')

local talk = conversation.new({
    variables = {name = 'Traveler'},
    nodes = {
        start = {
            steps = {{speaker = 'guard', text = 'Halt, {name}.'}},
            choices = {{text = 'I come in peace.', next = 'peace'}, {text = 'Leave.'}},
        },
        peace = {steps = {{speaker = 'guard', text = 'Then pass.'}}},
    },
})
print(talk:start('start').line.text) -- Halt, Traveler.
```

## Conversation

A `Conversation`, named `haylen.Conversation` in messages, is the graph of nodes that a file or [`conversation.new`](#conversationnewdocument) gives. It never changes, so one conversation can run in several walks at the same time. Reading a member that it does not have raises `The type "haylen.Conversation" has no member "<name>".`.

### talk:start(node, options)

Starts a new walk at the node named `node` and returns its [`ConversationState`](#conversationstate). The walk runs at once up to its first line, its first choices or its end. The table `options` is optional.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `variables` | table | `{}` | Values that replace the values of the file for this walk. A variable that the file lacks is added. |
| `seed` | integer | random | The seed of the generator that picks variants. The same seed picks the same variants. |

A missing node raises `The conversation has no node "<node>".`. An unknown option raises `Unknown option "<key>".`, and `variables` that are not a table of values by name, such as a list, raise `The variables of a conversation are an object of values by name.`. A `seed` that is not an integer raises a `bad argument` error. Commands that run during the start wait for the first listener of [`state:on`](#stateonevent-listener-options).

```lua
local assets = require('haylen.assets')

local talk = assets.load('dialogue/smith.conversation')
local state = talk:start('greeting', {variables = {gold = 12}, seed = 7})
print(state.line.key) -- smith.hello
```

### talk:restore(saved)

Returns a new [`ConversationState`](#conversationstate) that resumes from the table `saved`, which [`state:save()`](#statesave) returned. It stands at the same node and step, with the same line, choices, variables, visits, used choices and generator. A save still fits a conversation that changed since, such as after an update of the app, while the node it stands at and the choices it offered still exist. Visits of nodes that the conversation lost are dropped, and a step past the end of its node counts as the end.

A save that does not fit raises `The saved conversation does not fit this conversation: <reason>.`. The reason is one of these, or the message of the JSON reader when a key is missing or has the wrong type:

- `the conversation has no node "<node>"`
- `the node "<node>" has no choice <number>`
- `the state of its generator has the wrong size`
- `its variables are not an object`

```lua
local assets = require('haylen.assets')
local storage = require('haylen.storage')

local talk = assets.load('dialogue/smith.conversation')
local state = talk:start('greeting')
storage.writeSlot('smith', state:save())

local saved = storage.readSlot('smith')
if saved then
    local resumed = talk:restore(saved)
    print(resumed.node, resumed.line.key) -- greeting smith.hello
end
```

### talk.texts

Read-only list of every text of the lines and choices, each one once. It goes node by node, sorted by name, and in each node through the texts of its lines, every variant included, and then the texts of its choices. Translators turn these texts into the keys of the catalog. Every read returns a new table.

```lua
local assets = require('haylen.assets')

local talk = assets.load('dialogue/smith.conversation')
for _, text in ipairs(talk.texts) do
    print(text)
end
```

### talk.nodes

Read-only list of the names of the nodes, sorted by name. Every read returns a new table.

```lua
local assets = require('haylen.assets')

local talk = assets.load('dialogue/smith.conversation')
print(table.concat(talk.nodes, ', ')) -- buy, greeting, rumor
```

## ConversationState

A `ConversationState`, named `haylen.ConversationState` in messages, is one walk through a conversation, which [`talk:start`](#talkstartnode-options) and [`talk:restore`](#talkrestoresaved) return. At any moment it does one of three things:

- It shows a line. Then `state.line` holds the line, `state.choices` is empty, and `state:advance()` goes on.
- It waits for a choice. Then `state.choices` lists the choices, `state.line` holds the line before them if there is one, and `state:choose(index)` goes on.
- It has finished. Then `state.finished` is `true` and `state.line` is `nil`.

Steps that run in a row without a line, such as jumps that lead in a circle, stop after 10000 steps with `The conversation ran 10000 steps without a line in the node "<node>", which looks like a loop of jumps.`. The walk then counts as finished. Reading a member that a state does not have raises `The type "haylen.ConversationState" has no member "<name>".`, and writing one of its properties raises `The type "haylen.ConversationState" has no writable property "<name>".`.

### state.line

Read-only table of the line that the walk shows, or `nil` without one. Every read returns a new table.

| Field | Type | Meaning |
| --- | --- | --- |
| `text` | string | The text to show, translated by the catalog and with its placeholders filled. |
| `key` | string | The text as the file writes it, the variant that the generator picked for a list. |
| `speaker` | string | The speaker of the line, or `''` without one. |
| `tags` | list | The tags of the line, or an empty list without them. |

The last line stays while the choices that follow it show, so a question shows with its answers. It goes away when another line replaces it, when a choice is taken or when the walk ends.

```lua
local assets = require('haylen.assets')

local state = assets.load('dialogue/smith.conversation'):start('greeting')
local line = state.line
if line then
    print(line.speaker .. ': ' .. line.text)
    if #line.tags > 0 then
        print('The mood is ' .. line.tags[1] .. '.')
    end
end
```

### state.choices

Read-only list of the choices that the walk offers, in the order of the file. Each choice is a table `{text, key}`, where `text` is translated with its placeholders filled and `key` is the text of the file. It holds only the choices that their `if` allows and that are not used up, and it is empty while no choice waits. Every read returns a new table.

```lua
local assets = require('haylen.assets')

local state = assets.load('dialogue/smith.conversation'):start('greeting')
state:advance()
for index, choice in ipairs(state.choices) do
    print(index .. '. ' .. choice.text)
end
```

### state.finished

Read-only boolean that is `true` once the walk ended.

```lua
local assets = require('haylen.assets')

-- Takes the last choice, which leaves the smith.
local state = assets.load('dialogue/smith.conversation'):start('greeting')
state:advance()
state:choose(#state.choices)
print(state.finished) -- true
```

### state.variables

Read-only copy of the variables of the walk, as a table. Changing the copy does not change the walk, so use [`state:setVariable`](#statesetvariablename-value) instead. A variable set to `nil` is missing from the copy.

```lua
local assets = require('haylen.assets')

local state = assets.load('dialogue/smith.conversation'):start('greeting')
print(state.variables.gold) -- 30
```

### state.node

Read-only name of the node where the walk stands. After the walk ends, it names the node where it ended.

```lua
local assets = require('haylen.assets')

local state = assets.load('dialogue/smith.conversation'):start('greeting')
print(state.node) -- greeting
```

### state:advance()

Goes on from the line that the walk shows. It runs the steps up to the next line, the next choices or the end. Commands that run reach the listeners at the end of the call. It raises `The conversation waits for a choice. Call "choose" with one of its choices.` while choices wait, and `The conversation has finished. Start it again to go on.` once the walk ended, when only a new walk from `talk:start` goes on.

```lua
local assets = require('haylen.assets')

local state = assets.load('dialogue/smith.conversation'):start('greeting')
print(state.line.key) -- smith.hello
state:advance()
print(#state.choices) -- 3
```

### state:choose(index)

Takes the choice at the position `index` of `state.choices`, counted from 1. The line goes away, and a choice with `once` is used up. The walk then enters the node of the choice and runs up to its first line, its choices or its end. A choice without `next` ends the walk. Commands that run reach the listeners at the end of the call.

Without choices, it raises `The conversation offers no choices now.`. A position past the list raises `The conversation offers <count> choices, so there is no choice <index>.`, and a position below 1 raises `bad argument #1 to 'choose' (expected a choice from 1)`.

```lua
local assets = require('haylen.assets')

local state = assets.load('dialogue/smith.conversation'):start('greeting')
state:advance()
state:choose(1)
print(state.node, state.line.key) -- buy smith.thanks
```

### state:setVariable(name, value)

Sets the variable `name` of the walk to `value`, such as one that the app changed outside the conversation. The value is any value that JSON can hold: `nil`, a boolean, a number, a string or a table of those. Steps and choices that read the variable later see the new value, while choices already on offer stay as they are. A value that JSON cannot hold raises `A <type> cannot be converted to JSON.`.

```lua
local assets = require('haylen.assets')

-- The player starts poor and finds gold before the smith offers his wares.
local state = assets.load('dialogue/smith.conversation'):start('greeting', {variables = {gold = 5}})
state:setVariable('gold', 50)
state:advance()
print(#state.choices) -- 3
```

### state:visits(node)

Returns how often the walk entered the node `node`, by its start, a jump, a `next` or a choice. It returns `0` for a node that the walk never entered or that the conversation does not have.

```lua
local assets = require('haylen.assets')

local state = assets.load('dialogue/smith.conversation'):start('greeting')
state:advance()
state:choose(1)
state:advance()
print(state:visits('greeting')) -- 2
```

### state:save()

Returns a table of the whole walk: the node and step where it stands, the variables, the visits, the used choices, the line, the choices on offer and the state of the generator. It holds only plain values, so [`storage.writeSlot`](storage.md#storagewriteslotslot-data-summary) keeps it as it is, and [`talk:restore`](#talkrestoresaved) resumes it. Commands that no listener received yet are not part of it.

```lua
local assets = require('haylen.assets')
local storage = require('haylen.storage')

local state = assets.load('dialogue/smith.conversation'):start('greeting')
storage.writeSlot('slot-1', {conversation = state:save()}, {place = 'Forge'})
```

### state:on(event, listener, options)

Calls `listener` for every command that the walk runs, and returns a [`Connection`](signal.md#connection) of `haylen.signal`. The only event is `'command'`, which the [events](#events) section describes. Its method `disconnect()` stops the listener.

A listener runs at the end of the call that ran the command, such as `state:advance()`, so the walk already stands at its next line or choices, and the listener may call `advance` or `choose` itself. Commands wait until something listens, so the commands that ran before the first listener reach it during the call to `on`. An error raised in a listener is raised by the call that delivered the command.

The table `options` may hold an `owner`, a table or a userdata such as a scene, and the listener ends with it, as [subscription scopes](../core/lifecycle.md#subscription-scopes) describe. Without an `owner`, a listener that a module adds at its top level in development belongs to that module, so a [reload](hotReload.md) of the module ends it.

An unknown event raises `Unknown conversation event "<event>". Conversations report "command".`. An unknown option raises `Unknown option "<key>".`, and an owner of another type raises `An owner must be a table or a userdata, not <type>.`.

```lua
local assets = require('haylen.assets')

local state = assets.load('dialogue/smith.conversation'):start('greeting')
local commands = state:on('command', function(name, arguments)
    if name == 'wave' then
        print('The smith waves his ' .. arguments.hand .. ' hand.')
    end
end)
state:advance()
commands:disconnect()
```

## Events

The method [`state:on`](#stateonevent-listener-options) listens to this event.

| Event | Listener arguments | Meaning |
| --- | --- | --- |
| `'command'` | `name` (string), `arguments` (any value) | A command step ran. The `arguments` are the value of its `arguments` key, and an empty table when the step has none. |

## A conversation on screen

This scene shows the conversation of the [files](#files) section with [`haylen.ui`](ui.md). It shows the line, a button for each choice and a button to go on after a line. After every step it replaces the content of its GUI, and it reports the item of the command `giveItem`.

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
        self.gui = ui.mount(ui.column{id = 'talk'}, {owner = self})
        self:show()
    end,
    show = function(self)
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
        self.gui:replaceChildren('talk', rows)
    end,
})
```

## Errors

These messages are raised by the functions and methods. The [file errors](#file-errors) list the mistakes of a file.

| Message | Cause |
| --- | --- |
| `The conversation has no node "<node>".` | The method `talk:start` names a node that does not exist. |
| `The variables of a conversation are an object of values by name.` | The option `variables` of `talk:start` is a list or not a table. |
| `The saved conversation does not fit this conversation: <reason>.` | The save of `talk:restore` is broken or belongs to another conversation. |
| `The conversation waits for a choice. Call "choose" with one of its choices.` | The method `state:advance` ran while choices wait. |
| `The conversation has finished. Start it again to go on.` | The method `state:advance` ran after the walk ended. |
| `The conversation offers no choices now.` | The method `state:choose` ran while no choice waits. |
| `The conversation offers <count> choices, so there is no choice <index>.` | The position of `state:choose` is past the list. |
| `bad argument #1 to 'choose' (expected a choice from 1)` | The position of `state:choose` is below 1. |
| `The conversation ran 10000 steps without a line in the node "<node>", which looks like a loop of jumps.` | The walk ran in a circle without a line. |
| `The expression "<text>" uses "<operator>" with a value that is not a number.` | An expression did arithmetic or an order comparison on a value that is not a number. |
| `Unknown conversation event "<event>". Conversations report "command".` | The method `state:on` names another event. |
| `An owner must be a table or a userdata, not <type>.` | The option `owner` of `state:on` is another type. |
| `A <type> cannot be converted to JSON.` | A document, the variables or a value holds something that JSON cannot hold, such as a function. |
| `Unknown option "<key>".` | The options of `talk:start`, `state:on` or the load of the asset hold a key that they do not take. |
| `The type "<type>" has no member "<name>".` | The code read a member that a conversation or a state does not have. |
| `The type "<type>" has no writable property "<name>".` | The code wrote a property of a conversation or a state. |
