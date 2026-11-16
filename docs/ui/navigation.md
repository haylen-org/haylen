# 🧭 Navigation

This guide explains how an app moves between its screens: a frame with bars that stays, stacks that push and pop screens in the middle of the frame, tabs that each keep their own stack, a list and its detail that sit side by side on a tablet and work as one stack on a phone, and presentations that open a new flow over everything. It also explains the system back, deep links and the hooks of a screen. The [`haylen.ui` reference](../lua-api/ui.md#navigation) lists every function, option and error.

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

A presentation shows a screen over everything the app shows, such as a new post, a checkout or a sign in. It starts a new flow. Its screen may hold bars of its own and stacks of its own, and the GUIs under it take no input or focus until it leaves.

| Style | What it looks like |
| --- | --- |
| `sheet` | It rises from the bottom over a dimmed backdrop. On a narrow screen it fills the width up to the bottom edge, and on a wide screen it floats as a rounded card in the middle. The engine draws its surface in the theme `panel` color. |
| `fullScreen` | It rises over the whole screen, on the theme `background` color, and lays out its screen in the safe area. |
| `dialog` | It fades in over a dimmed backdrop. Its screen lays out in the safe area and draws its own card, usually a root with `align = 'center'`. |

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

A tap on the backdrop dismisses a dismissible sheet or dialog. A presentation with `dismissible = false` stays until the app dismisses it, and the system back does nothing while it shows. A stack whose area lies in a presented screen closes when the presentation leaves.

## The system back

The system back is the back button of Android and of a gamepad, Escape, the east button of a gamepad and the menu button of a TV remote. A focus scope that takes it, such as an open dialog or popup, takes it first. Then the navigation takes it, in this order:

1. The innermost stack that shows and has a screen under its top one pops. In a split, the detail goes back before the list.
2. Tabs whose selected tab is not the first go to the first tab.
3. The topmost presentation is dismissed.

The screen that a pop or a dismissal would take away hears `back` first and may keep its place. Only when nothing goes back does the press reach the app: the root of the GUI with the focus hears `cancel`, and on Android the back button leaves the app. So an app built on the navigation never calls `window.setBackLeavesApp`. A back button in a bar calls `ui.goBack()`, which goes back the same way, and `ui.canGoBack()` tells whether it would.

## Deep links

A link that opens the app, such as a link to a conversation, builds the stack that the person would have reached by hand. The tabs select the tab, and the stack of that tab takes the whole list at once. Only the top screen builds its GUI, and the screens under it build theirs when the person goes back to them.

```lua
local events = require('haylen.events')

events.on('linkOpened', function(event)
    local person = event.url:match('^photos://chat/(%w+)$')
    if person then
        tabs:select('messages')
        tabs.current:setScreens({Inbox(), Chat(person)}, {effect = 'none'})
    end
end)
```

## Scenes and navigation

Scenes and navigation work at two levels. A [scene](../core/lifecycle.md#scene-lifecycle) is a whole flow of the app, such as the sign in, the main app or a level of a game, with its own frame. The navigation moves between the screens inside a scene. A frame that a scene owns, and every stack in it, leaves with the scene, and a scene transition moves the frame and its screens together. A frame that no scene owns, such as the frame of an autoload, stays above the scenes. Use a scene change when the whole app changes, and a stack, tabs or a presentation when only the middle changes.

## Cost

A stack draws only the top screen of each stack that shows, and both screens of a change that plays. A screen that stays still costs nothing more than any other GUI, and a frame in which nothing moves allocates nothing in the navigation. The screens of hidden tabs keep their GUIs in memory and draw nothing. A transition moves the GUIs of two screens in their area with a clip, so its cost is the cost of drawing those two GUIs.

## From C++

The classes of `haylen::ui` are the same model. `ui::Screen` builds its GUI in `build` and hears the same hooks. The `ui::Navigation` of the UI plugin creates stacks with `createStack`, tabs with `createTabs`, splits with `createSplit` and presentations with `present`, and `goBack` takes the system back.

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
