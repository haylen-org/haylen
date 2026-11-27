# 💬 Conversation

This guide explains how an app runs branching conversations with the module `haylen.conversation`, such as the dialogue of characters, a tutorial or the steps of onboarding. It covers how a conversation is written as a file, how a walk moves through it, how the writing talks to the game through variables and commands, how texts reach translators and how a walk is saved and resumed. The [`haylen.conversation` reference](../lua-api/conversation.md) describes every key of the file format, every function and every error.

## Conversations and walks

Two objects work together. A [`Conversation`](../lua-api/conversation.md#conversation) is the writing: a graph of named nodes that a `.conversation` file of the package holds. It never changes once it is loaded. A [`ConversationState`](../lua-api/conversation.md#conversationstate) is one walk through that graph: where the reader stands, the values of the variables, how often each node was visited and which choices are used up.

Because the writing never changes, one conversation can run in many walks at once. A shopkeeper can talk to two players in a split screen, or a test can walk every branch while the game runs another walk.

```lua
local assets = require('haylen.assets')

local talk = assets.load('dialogue/innkeeper.conversation')
local first = talk:start('welcome', {seed = 1})
local second = talk:start('welcome', {seed = 2})
print(first.node, second.node) -- welcome welcome
```

A conversation loads as an asset of the type `conversation`, at once with `assets.load` or on a worker thread with `assets.loadAsync`, as [Loading](../lua-api/conversation.md#loading) explains. A Lua table in the same format becomes a conversation with [`conversation.new`](../lua-api/conversation.md#conversationnewdocument), which suits dialogue that the app builds itself, such as the lines of a generated quest.

## Writing a conversation

A file is a JSON object with its `nodes` and, optionally, the starting values of its `variables`. Each node is one beat of the talk. It runs its steps in order, then offers its choices, and with no choice to offer it moves on to its `next` node or ends.

A step is one of four kinds. A line shows a text with an optional speaker and tags, and the walk stops there until the reader goes on. A command asks the game to do something. A set changes variables. A jump moves to another node at once. Any step and any choice can carry an `if`, a small expression that must be true for it to run or show. The [Files](../lua-api/conversation.md#files) section of the reference lists every key.

```json
{
    "variables": {"coins": 4, "room": false},
    "nodes": {
        "welcome": {
            "steps": [
                {"speaker": "innkeeper", "text": "inn.welcome", "tags": ["smile"]},
                {"speaker": "innkeeper", "text": "inn.back", "if": "visits.welcome > 1"}
            ],
            "choices": [
                {"text": "inn.askRoom", "if": "not room", "next": "room"},
                {"text": "inn.askNews", "next": "news", "once": true},
                {"text": "inn.bye"}
            ]
        },
        "room": {
            "steps": [
                {"jump": "poor", "if": "coins < 3"},
                {"set": {"coins": "coins - 3", "room": true}},
                {"command": "giveKey", "arguments": {"door": 2}},
                {"speaker": "innkeeper", "text": "inn.roomGiven"}
            ],
            "next": "welcome"
        },
        "poor": {"steps": [{"speaker": "innkeeper", "text": "inn.tooPoor"}], "next": "welcome"},
        "news": {"steps": [{"speaker": "innkeeper", "text": ["inn.news.wolves", "inn.news.storm"]}], "next": "welcome"}
    }
}
```

The conditions read like Lua: `and`, `or`, `not`, comparisons and arithmetic on numbers. The word `visits` followed by a node name counts how often the walk entered that node, so a character can greet a returning reader in a different way without a variable for it. The [Expressions](../lua-api/conversation.md#expressions) section gives the exact rules, such as the one place where the order of operators differs from Lua.

A text that is a list, such as the news above, is a variant. Each time the step runs, the walk picks one text of the list. The walk picks with a generator that the option `seed` of [`talk:start`](../lua-api/conversation.md#talkstartnode-options) fixes, so the same seed picks the same variants in every run, which keeps replays and tests the same.

A file is checked as a whole when it loads. A node that a `next` or a `jump` names but the file lacks, a key that does not belong or a condition that does not parse stops the load with a message that names the node and the step. So a typing mistake of the writer shows at once and never in the middle of a play. The [file errors](../lua-api/conversation.md#file-errors) list every message.

## Walking

A walk always stands in one of three places. It shows a line, it waits for a choice or it has finished. The app reads [`state.line`](../lua-api/conversation.md#stateline) and [`state.choices`](../lua-api/conversation.md#statechoices), shows them, and moves on with [`state:advance`](../lua-api/conversation.md#stateadvance) after a line or [`state:choose`](../lua-api/conversation.md#statechooseindex) when choices wait. Between those calls the walk does nothing, so the reader takes all the time they want.

The last line stays while its choices show. So a question and its answers appear on screen together, which is how most dialogue boxes look.

```lua
local assets = require('haylen.assets')

local function describe(state)
    if state.finished then
        return 'The conversation ended.'
    end
    local parts = {}
    if state.line then
        parts[#parts + 1] = state.line.speaker .. ': ' .. state.line.text
    end
    for index, choice in ipairs(state.choices) do
        parts[#parts + 1] = index .. '. ' .. choice.text
    end
    return table.concat(parts, '\n')
end

local state = assets.load('dialogue/innkeeper.conversation'):start('welcome')
print(describe(state))
state:advance()
print(describe(state))
```

The module draws nothing. The app shows lines and choices with the components of [`haylen.ui`](../lua-api/ui.md), with text in a canvas or with anything else, and the tags of a line pick a portrait, a voice or an animation. The reference holds a complete scene that shows a walk with buttons in [A conversation on screen](../lua-api/conversation.md#a-conversation-on-screen).

## Talking to the game

Variables and commands connect the writing with the rest of the app, in both directions.

Variables carry facts. A walk starts with the values of the file, and the option `variables` of `talk:start` replaces them with the values of the game, such as the coins the player holds now. While the walk runs, [`state:setVariable`](../lua-api/conversation.md#statesetvariablename-value) passes in a change that happened outside it. When the walk ends, [`state.variables`](../lua-api/conversation.md#statevariables) holds what the writing changed, which the game copies back. The game stays the owner of its state, and each walk works on its own copy.

Commands carry actions. A command step hands its name and its arguments to the listeners of [`state:on`](../lua-api/conversation.md#stateonevent-listener-options), and the game gives an item, plays a sound or starts a cutscene. A listener runs once the call that ran the command returns, so the walk already stands at its next line when the game acts. Give the listener an `owner`, such as the scene, so it ends with that owner.

```lua
local assets = require('haylen.assets')
local scene = require('haylen.scene')

local player = {coins = 10, keys = {}}

scene.push({
    enter = function(self)
        local talk = assets.load('dialogue/innkeeper.conversation')
        self.state = talk:start('welcome', {variables = {coins = player.coins}})
        self.state:on('command', function(name, arguments)
            if name == 'giveKey' then
                player.keys[#player.keys + 1] = arguments.door
            end
        end, {owner = self})
    end,
    exit = function(self)
        player.coins = self.state.variables.coins
    end,
})
```

Keep game rules in the game and the shape of the talk in the file. The file decides who says what and which branch follows. The game decides what an item does, so a command names an action, and the listener carries it out.

## Texts and translation

The text of a line or a choice is a key of the [localization](localization.md) catalog. When the catalog has no such key, the text shows as it is written. So a writer can draft with plain sentences, and translators turn them into keys later. The property [`talk.texts`](../lua-api/conversation.md#talktexts) lists every text of a conversation once, every variant included, which gives translators the full list to work from.

Texts fill their `{name}` placeholders with the variables of the walk, as [Texts](../lua-api/conversation.md#texts) explains, so `"You have {coins} coins."` shows the current value. Each line keeps both forms: `text` is what to show, and `key` is what the file wrote, which suits picking a recorded voice for the line.

```lua
local assets = require('haylen.assets')

local talk = assets.load('dialogue/innkeeper.conversation')
local missing = {}
for _, text in ipairs(talk.texts) do
    if not text:find('^inn%.') then
        missing[#missing + 1] = text
    end
end
print(#missing .. ' texts are not keys of the catalog yet.')
```

## Saving and resuming

A walk can stop anywhere and come back later, even after the app quits. The method [`state:save`](../lua-api/conversation.md#statesave) returns a table of plain values with everything the walk knows, and [`talk:restore`](../lua-api/conversation.md#talkrestoresaved) builds a walk that stands exactly where the saved one stood, with the same line, choices, variables, visits and generator. The table goes straight into a save slot of [`haylen.storage`](storage.md).

```lua
local assets = require('haylen.assets')
local storage = require('haylen.storage')

local talk = assets.load('dialogue/innkeeper.conversation')
local state = talk:start('welcome')
state:advance()
storage.writeSlot('autosave', {innkeeper = state:save()}, {place = 'Inn'})

local saved = storage.readSlot('autosave')
local resumed = saved and talk:restore(saved.innkeeper)
if resumed then
    print(resumed.node, #resumed.choices)
end
```

A save survives an update of the app that changes the writing, as long as the node it stands at and the choices it offered still exist. When an update removes them, `talk:restore` raises an error that says why, and the app can start the walk again from a node it chooses.

## Checking a conversation

Conversations grow large, and a branch that nobody reads hides mistakes. Since walks are cheap and a seed makes them repeatable, a test or a debug command can walk every branch without a person. This function starts a walk at every node and takes each choice in turn, and it reports a walk that raises an error, such as a condition that compares a text with a number.

```lua
local assets = require('haylen.assets')

local function settle(state)
    while state.line and #state.choices == 0 and not state.finished do
        state:advance()
    end
end

local function walkFrom(talk, node, path, depth)
    local state = talk:start(node, {seed = 1})
    settle(state)
    for _, index in ipairs(path) do
        state:choose(index)
        settle(state)
    end
    if depth == 0 then
        return
    end
    for index = 1, #state.choices do
        local longer = {table.unpack(path)}
        longer[#longer + 1] = index
        walkFrom(talk, node, longer, depth - 1)
    end
end

local talk = assets.load('dialogue/innkeeper.conversation')
for _, node in ipairs(talk.nodes) do
    local ok, failure = pcall(walkFrom, talk, node, {}, 3)
    if not ok then
        print('The node ' .. node .. ' fails: ' .. tostring(failure))
    end
end
```

A walk that runs steps in a circle without ever showing a line stops after a bounded number of steps with an error that names the node, so a loop of jumps in the writing never freezes the app.
