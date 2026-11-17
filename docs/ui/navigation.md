# 🧭 Navigation

This guide explains how an app moves between its screens: a frame with bars that stays, stacks that push and pop screens in the middle of the frame, tabs that each keep their own stack, a list and its detail that sit side by side on a tablet and work as one stack on a phone, presentations that open a new flow over everything, such as sheets, popovers, drawers, dialogs, alerts and action sheets, and the toasts, snackbars and banners that tell of what happened. It also explains the results of a flow, unsaved work, the system back, the routes of links and the hooks of a screen. The [`haylen.ui` reference](../lua-api/ui.md#navigation) lists every function, option and error.

## Screens, stacks and the frame

An app is built from screens. A screen is one page, such as a feed, a profile or one step of a new post. The bars around the screens, such as a tab bar, a sidebar, a top bar or the player of a music app, belong to the frame. The frame is a GUI that the app mounts once. It holds an empty node where the screens go.

A navigation stack shows screens in the area of that node. Only the top screen shows. A push puts a new screen on top, and a pop takes the top screen away. The stack plays the transition of the change inside its area, so the screens slide under the bars while the bars stay still. The frame is never built again, so its state, its focus and its scroll stay as they are.

```lua
local haylen = require('haylen')
local ui = require('haylen.ui')

local Profile = haylen.class('Profile', ui.Screen)

function Profile:init(name)
    self.name = name
end

-- Returns the node table of the GUI of the screen, which the stack builds the first time the screen shows.
function Profile:build()
    return ui.column{
        ui.label{text = self.name, font = 'title'},
        ui.button{text = 'Open a friend', onClick = function()
            self.stack:push(Profile('A friend of ' .. self.name))
        end},
    }
end

local frame = ui.mount(ui.column{gap = 0,
    ui.row{height = 96, ui.button{text = 'Back', onClick = ui.goBack}},
    ui.column{id = 'middle', grow = 1},
}, {sharedFocus = true})

ui.navigationStack{gui = frame, id = 'middle', root = Profile('Ana')}
```

A screen is a table, usually made by a class of [`haylen.class`](../lua-api/haylen.md) that extends `ui.Screen`. Its `build` method returns the node table of its GUI. The engine sets three fields on the screen: `gui` is its GUI once it is built, `stack` is the stack that holds it, and `presentation` is the presentation that shows it. A screen shares the focus with the frame, so a remote or a gamepad moves from the screen to the bars and back.

## What a stack does

A stack changes by a new list of screens. Every operation is one of these lists:

- `stack:push(screen)` adds a screen on top.
- `stack:pop()` takes the top screen away.
- `stack:popTo(screen)` takes away every screen above a screen.
- `stack:popToRoot()` keeps only the first screen.
- `stack:replace(screen)` puts a screen in the place of the top one, such as the next step of a form.
- `stack:setScreens(list)` sets the whole list at once, as a deep link does.

A change moves the screens only once the GUI of the screen that comes laid out and every picture it shows arrived, so no picture pops in while the screen slides or rises. Until then the screen that comes waits out of sight, and the screen that leaves stays still. A change waits half a second at most, so a picture that takes longer, or never arrives, cannot hold the app: the change then plays, and the late picture shows once it arrives. A presentation waits for its screen the same way.

A change goes forward when the new top screen is new, and back when it was already in the stack. A forward slide comes in from the end of the reading direction, and a back slide comes in from its start. A right to left interface mirrors both. The screens of a change take no input until it ends. A change that starts while another one plays finishes the other one at once, so a fast series of taps never leaves screens halfway.

Each operation takes a transition table as its last argument, such as `{effect = 'fade', duration = 0.2}`. The effects are `slide`, `fade`, `rise` and `none`. A `rise` brings the screen up from the bottom of the screen, which suits a player that opens over a list. While the system asks for reduced motion, a slide and a rise cross fade instead.

A popped screen leaves for good. Its GUI unmounts, and everything it owns ends, such as its timers, its listeners and its tasks. A screen that stays under another one keeps its GUI, with its scroll and its fields, and shows it again when the screens above it leave.

## The life of a screen

A screen hears hooks as it lives, and all of them are optional except `build`. It builds its GUI the first time it shows. It hears `appear` when it starts to show and `appeared` once its transition ended, and `disappear` and `disappeared` as it leaves the screen, which happens every time another screen covers it or its tab hides. While it shows, `update` runs every frame, and `render` draws what it shows besides its GUI. The system back asks `back` before it takes the screen away, and `remove` tells the screen that it left for good. The [reference of `ui.Screen`](../lua-api/ui.md#uiscreen) lists every hook and field.

The function `ui.screenState(screen)` returns where the screen is: `created`, `appearing`, `shown`, `disappearing`, `hidden` or `removed`.

```lua
local events = require('haylen.events')

function Inbox:appear()
    -- Listens only while the screen shows, and the listener ends with the screen.
    events.on('messageArrived', function(message) self:add(message) end, {owner = self})
end
```

## Tabs that keep their stacks

Tabs give every tab a stack of its own in one area. Only the stack of the selected tab shows. The other stacks keep their screens, with their scroll, their focus and their fields, until their tab shows again. Each tab names a function that makes its root screen, which runs the first time the tab shows.

```lua
local tabs = ui.navigationTabs{gui = frame, id = 'middle', selected = 'home',
    tabs = {
        {id = 'home', root = function() return Home() end},
        {id = 'search', root = function() return Search() end},
    },
    onChange = function(event)
        frame:set('tab-' .. event.previous, {iconColor = 'textMuted'})
        frame:set('tab-' .. event.selected, {iconColor = 'accent'})
    end,
}

tabs:select('search')
```

The tab bar is part of the frame, so the app changes only the look of its buttons with `gui:set` when the selection changes, and never builds the bar again. Selecting the tab that shows pops its stack to the root, the way native tab bars do. The system back on the root of another tab goes to the first tab.

## A list and its detail

A split shows a list and its detail, such as the conversations and the open chat. It has two areas: `primary` for the list and `detail` for the detail. On a wide screen, the split is expanded. The list and the detail show side by side, and a placeholder shows in the detail area while nothing is picked. On a narrow screen, the split is collapsed. The detail screens move on top of the list, in the area of the list, so they work as one stack with a slide and the system back. When the window grows again and the split expands, the detail screens move back to the detail area with their GUIs and their state.

```lua
local split = ui.navigationSplit{
    primary = {gui = frame, id = 'list'},
    detail = {gui = frame, id = 'detail'},
    root = Inbox(),
    placeholder = function() return PickAChat() end,
    collapsed = not wide,
}

-- Shows a chat beside the list on a tablet, or slides it in over the list on a phone.
split:showDetail(Chat(person))
```

The app decides when the split collapses, in the same place where it lays out its frame for the width of the window. It sets `split.collapsed` and hides the detail node of the frame when there is no room for it. A split can live inside a screen too: the screen holds the two areas, so a tab of a tablet shows a list and a detail while the tab bar stays.

## Presentations open a new flow

A presentation shows a screen over everything the app shows, such as a new post, a checkout, a picker or a sign in. It starts a new flow. Its screen may hold bars of its own and stacks of its own. A modal presentation takes the input and the focus of the GUIs under it until it leaves, and then the focus goes back to the control that had it. Screen readers reach only the modal presentation and what lies above it until it leaves.

| Style | What it looks like | When to use it |
| --- | --- | --- |
| `sheet` | A surface that rises from the bottom over a dimmed backdrop. | A task that the person finishes or drops, such as a new post, the queue of a player or the details of a ride. |
| `fullScreen` | A surface over the whole screen, on the theme `background` color. | A flow that needs the whole screen and may not be dragged away, such as a camera or a sign in. |
| `dialog` | The screen fades in over a dimmed backdrop and draws its own card, usually a [`dialog`](../lua-api/ui.md#uidialogproperties). | A short question with a few fields, such as the name of a new playlist. |
| `drawer` | A panel the height of the screen that slides in from the start edge. | The places of an app on a phone, such as a menu of sections. |
| `popover` | A floating card with an arrow that points at a control, and a sheet on a compact screen. | Choices or details of one control, such as the colors of a pen or the options of a filter. |

```lua
local NewPost = haylen.class('NewPost', ui.Screen)

-- A flow of its own: a top bar with Cancel, and a stack of steps under it.
function NewPost:build()
    return ui.column{gap = 0,
        ui.row{height = 88, ui.button{text = 'Cancel', onClick = function() self.presentation:dismiss() end}},
        ui.column{id = 'steps', grow = 1},
    }
end

function NewPost:appear()
    if not self.steps then
        self.steps = ui.navigationStack{gui = self.gui, id = 'steps', root = PickPicture(), owner = self}
    end
end

ui.present(NewPost(), {style = 'sheet', onDismiss = function() print('The new post closed.') end})
```

A screen is compact while its safe area is narrower than a form sheet with its margins, such as a phone in portrait. The width counts in UI units and in points of the screen, so a phone is compact also when its UI lays out a wide design. The same presentation adapts to the screen it shows on: a sheet fills the width of a compact screen and floats as a card on a wide one, and a popover becomes a sheet on a compact screen. When the window changes size, such as a tablet that turns or a split screen, the presentation lays out again at once.

A tap on the backdrop, Escape, the system back and a drag dismiss a dismissible presentation. A presentation with `dismissible = false` stays until the app dismisses it.

## Sheets and their detents

A detent is a height a sheet rests at. A sheet lists its detents from the lowest to the highest: `'medium'` rests at half the screen, `'large'` near the top, `'fit'` at the height its screen measures, and a number at that height in UI units. A sheet with more than one detent shows a grabber along its top while it rests at its detents. The person drags the grabber, or any part of the sheet that no control takes, and the sheet glides to the detent nearest to where it was heading. A tap on the grabber moves it to the next detent, and screen readers read the grabber as a button that does the same. A drag well below the lowest detent dismisses the sheet.

```lua
-- The queue rests at half the screen, and the person pulls it up to see more.
local queue = ui.present(Queue(), {detents = {'medium', 'large'}, onDetentChange = function(event)
    queue.screen:showMore(event.detent == 2)
end})
```

On a compact screen, the GUIs behind a modal sheet shrink a little as it nears the top, so the sheet reads as a card over the screen it came from. On a wide screen, a form sheet floats as a card in the middle. It rests at no detent, so it shows no grabber, and it keeps one size: the height its screen measures when every detent is `'fit'` or a number, such as a picker, and otherwise the height a form sheet grows to. A page sheet keeps its detents along the bottom with a bounded width, which suits a sheet that the person resizes, such as the details of a ride on a tablet.

A sheet with `modal = false` leaves the input of the app to the GUIs under it and draws no backdrop, such as a map whose ride details stay open while the person moves the map. The system back moves such a sheet down to its lowest detent, and goes on to the app once it rests there.

## Popovers, drawers and context menus

A popover points at the node of its `anchor` with an arrow. It sits under the node when its screen fits there, and over it otherwise, and it is as tall as its screen measures. On a compact screen, it is a sheet instead, since a small card would be hard to reach there.

```lua
ui.present(Filters(), {style = 'popover', anchor = {gui = frame, id = 'filter'}, width = 520})
```

A drawer slides in from the start edge, which is the right edge of a right-to-left interface. It holds the places of an app on a phone, where a sidebar has no room. A drag toward its edge or a tap on the backdrop closes it.

A context menu belongs to a control rather than to the flow, so it is the [`contextMenu`](../lua-api/ui.md#uicontextmenuproperties) node of the GUI. It opens with a right click, a long press or the menu key, and closes with the system back before the navigation goes back.

## Alerts and action sheets

An alert interrupts with a decision, such as whether to delete a draft. An action sheet offers the choices of one control, such as the ways to share a picture. The engine builds both from a title, a message and a list of actions, so they look and behave the same in every app, with the keyboard, a gamepad, a remote and a screen reader.

```lua
local answer = ui.presentAlert{
    title = 'Delete the draft?',
    actions = {{id = 'keep', text = 'Keep', role = 'cancel'}, {id = 'delete', text = 'Delete', role = 'destructive'}},
}.result:await()
```

Every action has a role. The cancel action leads, and the person also chooses it by dismissing the alert. A destructive action shows in the danger color. The last default action is the preferred one and starts with the focus. An alert without a cancel action stays until the person picks an action, such as an update that the app needs.

An action sheet points at its anchor as a popover on a wide screen, and rises from the bottom of a compact one as a sheet as tall as its actions.

## Messages

A message tells the person of something that happened and goes away on its own, without a decision and without the focus, so the person goes on with what they were doing. The navigation shows messages above every GUI and every presentation, in three styles that follow the platforms people know:

- A toast confirms something short, such as a saved file, in a pill at the bottom middle that fits its text. A few toasts show together, and each lasts 3 seconds.
- A snackbar tells of an action that the person may take back, such as an undo after a deletion, in a bar across the bottom with its action at the end. One snackbar shows at a time, for 4 seconds, or 6 with an action.
- A banner tells of news that the person may open, such as a chat message, in a card that drops in under the status bar with a picture at its start. One banner shows at a time, for 5 seconds, and a tap on it reports that the person wants to see more.

```lua
ui.showMessage{text = 'Saved to your collection.', tone = 'success', indicator = 'icon'}

ui.showMessage{style = 'snackbar', text = 'Removed from your collection.', action = 'Undo', onAction = function()
    Collection.add(post)
end}

ui.showMessage{style = 'banner', title = 'Ana', text = 'See you at six?', icon = 'images/ana.png', onTap = function()
    openChat('ana')
end}
```

The messages lie in the safe area. An app whose frame holds bars, such as a tab bar or the player of a music app, keeps them inside the middle of its frame with `ui.setMessageArea`, so a snackbar never covers its tabs. The area counts while its node draws and no modal presentation covers it, and the messages go back to the safe area over a presentation, which covers the bars anyway.

```lua
ui.setMessageArea{gui = frame, id = 'middle'}
```

Messages of one position stack without covering each other, as a list or as a deck with the newest in front. The others wait their turn in a queue, a higher priority before a lower one, and a snackbar or a banner of a higher priority replaces the one that shows, such as a warning that the connection dropped over a confirmation. The time of a message stops while the pointer or a finger rests on its stack, and the person taps it, swipes it away, closes it or presses its action. Its `result` resolves with the reason it left, so a snackbar can wait for the undo before it deletes for good. Screen readers hear every message that comes on screen, and a high priority interrupts them. They read the action and the close button of a message as buttons, which they press though neither takes the focus. Use an alert instead when the person must decide before going on.

## Results of a flow

A presented flow can hand a value back to whoever presented it, such as the color the person picked or the post they wrote. The screen dismisses its presentation with a result, and the `result` of the presentation is a promise that resolves with it once the presentation left. A dismissal by the person resolves it with `nil`, and an alert resolves it with the id of the action.

```lua
function ColorPicker:pick(color)
    self.presentation:dismiss({result = {color = color}})
end

async.spawn(function()
    local picked = ui.present(ColorPicker(), {detents = {'fit'}}).result:await()
    if picked then
        pen.color = picked.color
    end
end)
```

A result is a value that JSON holds, the way the results of other screens of the system are plain data, so it can also be saved with the state of the navigation.

## Unsaved work

A screen that holds unsaved work, such as a draft, keeps its place with its `back` hook. The hook hears every way the person goes back: the system back, a tap on the backdrop and a drag that would dismiss the sheet. It returns `true` to keep the screen and asks first, and the sheet glides back to its detent.

```lua
function Compose:back()
    if self.text == '' then
        return false
    end

    async.spawn(function()
        local answer = ui.presentAlert{title = 'Discard the draft?', actions = {
            {id = 'keep', text = 'Keep editing', role = 'cancel'},
            {id = 'discard', text = 'Discard', role = 'destructive'},
        }}.result:await()
        if answer == 'discard' then
            self.presentation:dismiss()
        end
    end)
    return true
end
```

## The system back

The system back is the back button of Android and of a gamepad, Escape, the east button of a gamepad and the menu button of a TV remote. A focus scope that takes it, such as an open dialog or popup, takes it first. Then the navigation takes it, in this order:

It goes through the layers from the top: each presentation, and then the app under them. In each layer:

1. The innermost stack that shows and has a screen under its top one pops. In a split, the detail goes back before the list.
2. Tabs whose selected tab is not the first go to the first tab.
3. A modal presentation is dismissed, when it is dismissible, and keeps the back from the layers under it either way. A sheet that is not modal goes down to its lowest detent, and once it rests there the back goes on to the layer under it.

The screen that a pop or a dismissal would take away hears `back` first and may keep its place. Only when nothing goes back does the press reach the app: the root of the GUI with the focus hears `cancel`, and on Android the back button leaves the app. So an app built on the navigation never calls `window.setBackLeavesApp`. A back button in a bar calls `ui.goBack()`, which goes back the same way, and `ui.canGoBack()` tells whether it would.

## Routes for links

A link that opens the app, such as a link to an album that a friend sent, lands on the screen it names, with the screens the person would have reached by hand under it. The routes of the app decide that state. A route has a path whose segments may be typed parameters, such as `album/{id:integer}`, and typed query parameters. It names the tab it shows in, the route whose screens lie under it, and whether its screen is a detail or a presentation.

```lua
local router = ui.router{tabs = tabs, schemes = {'music'}, hosts = {'music.example.com'}, restore = true, routes = {
    {name = 'home', path = 'home', tab = 'home'},
    {name = 'artist', path = 'artist/{id}', tab = 'search', screen = function(p) return Artist(p.id) end},
    {name = 'album', path = 'album/{id:integer}', parent = 'artist', screen = function(p) return Album(p.id) end},
    {name = 'chat', path = 'chat/{person}', tab = 'messages', detail = true, screen = function(p) return Chat(p.person) end},
    {name = 'player', path = 'player', present = {detents = {'large'}}, screen = function() return Player() end},
}}
```

The link `music://album/7?artist=42` reads as the location `album/7`, since the host of a link of a scheme of the app is the first segment of its path. A web link of a host of the app, such as `https://music.example.com/album/7`, reads its path the same way. The first route that matches takes the link. A segment that does not read as the type of its parameter makes the route not match, so `album/seven` reaches no album.

The router opens every link that [`linkOpened`](../core/lifecycle.md#the-whole-lifecycle) reports, the one that launched the app and those that arrive while it runs, through the same flow that the platform and the plugins use for links. A link that opens the app before it creates its router, such as the link that launched it while its first scene loads, waits for the first router that opens links, which opens the last such link once the app registered it. Opening a link builds the whole state: the modal presentations leave, the tab of the route shows, and its stack holds the root of the tab, the screens of the parents of the route and the screen of the route. A route with `detail` shows its screen as the detail of the split that the screen under it holds, and a route with `present` presents it over them. A link that no route matches leaves the app as it is, and the router logs `No route matches the link "<link>".` and tells `onUnmatched`.

Code goes to a route the same way. `router:open(location)` builds the whole state like a link, and `router:push(location)` shows the screen of the location in the flow that shows, such as a button that opens an album from an artist.

## Keeping the state of the navigation

Mobile systems end apps in the background to free memory, and a person who comes back expects the app where they left it. A router with `restore = true` keeps the state of the navigation: the selected tab and the locations of the screens it showed in every stack, as details and as presentations. The engine writes it to the storage of the app when the app goes to the background and when it restarts, such as after a change of its code in development, and drops it when the app comes back to the foreground or stops normally. A launch after a restart or after the system ended the app builds the state again, each screen from its location, and a link that launched the app opens after it.

Only the screens that the router showed have a location, so an app that wants its whole state back shows its screens through its routes. A screen that keeps more, such as a draft or the scroll of a list, keeps it with the [resume state](../lua-api/storage.md#resume-state) of `haylen.storage`.

## Scenes and navigation

Scenes and navigation work at two levels. A [scene](../core/lifecycle.md#scene-lifecycle) is a whole flow of the app, such as the sign in, the main app or a level of a game, with its own frame. The navigation moves between the screens inside a scene. A frame that a scene owns, and every stack in it, leaves with the scene, and a scene transition moves the frame and its screens together. A frame that no scene owns, such as the frame of an autoload, stays above the scenes. Use a scene change when the whole app changes, and a stack, tabs or a presentation when only the middle changes.

## Cost

A stack draws only the top screen of each stack that shows, and both screens of a change that plays. A presentation draws its screen and its surface, and a sheet that moves lays its screen out again at its new height. A message draws in an ImGui window of its own above everything. A screen or a message that stays still costs nothing more than any other GUI, and a frame in which nothing moves allocates nothing in the navigation. The screens of hidden tabs keep their GUIs in memory and draw nothing. A transition moves the GUIs of two screens in their area with a clip, so its cost is the cost of drawing those two GUIs.

## From C++

The classes of `haylen::ui` are the same model. `ui::Screen` builds its GUI in `build` and hears the same hooks. The `ui::Navigation` of the UI plugin creates stacks with `createStack`, tabs with `createTabs`, splits with `createSplit`, presentations with `present`, alerts with `presentAlert`, messages with `showMessage` in the area of `setMessageArea` and the routes of links with `createRouter`, which takes the options of `ui.showMessage` as JSON properties, and `goBack` takes the system back. A presentation tells that it left with its `dismissed` signal, and `getResult` returns its result as JSON. A `ui::Message` tells what happened with its signals `appeared`, `tapped`, `actionPressed` and `dismissed`, and `getReason` says why it left.

```cpp
#include <memory>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/Screen.hpp"

class Home final : public haylen::ui::Screen {
  public:
    std::shared_ptr<haylen::ui::Gui> build(haylen::core::Engine& engine, haylen::ui::Placement placement) override {
        const haylen::core::Json tree = {{"kind", "label"}, {"text", "Home"}};
        return engine.getPlugin<haylen::plugins::UiPlugin>().createGui(tree, placement);
    }
};

class Frame final {
  public:
    // Shows a home screen in the middle of the frame, and pushes another one over it with a slide.
    static void openHome(haylen::core::Engine& engine, const std::shared_ptr<haylen::ui::Gui>& frame) {
        haylen::ui::Navigation& navigation = engine.getPlugin<haylen::plugins::UiPlugin>().getNavigation();
        const std::shared_ptr<haylen::ui::NavigationStack> stack = navigation.createStack({.gui = frame, .node = "middle"}, std::make_shared<Home>());
        stack->push(std::make_shared<Home>(), {.effect = haylen::ui::NavigationTransition::Effect::Slide});
    }
};
```
