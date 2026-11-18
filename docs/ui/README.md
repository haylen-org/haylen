# 🪟 UI

The UI draws the interface players see and the tools developers use, in design units, inside the safe area and with themes.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| Navigation | How an app moves between its screens: a frame whose bars stay, stacks that push and pop screens in its middle, tabs that keep their stacks, a list and its detail that collapse into one stack on a phone, presentations that open a new flow, the system back and deep links. | [`navigation.md`](navigation.md) |
| Text input | How text fields edit through the native text input of each platform, with input methods, composition and on-screen keyboards, and what each platform does. | [`text-input.md`](text-input.md) |
| UI | How to build a GUI step by step: mount it, lay it out, place it in the safe area, react to the player, theme it, move the focus with every device, share the screen with the game and scroll long lists, plus immediate mode windows for tools. | [`ui.md`](ui.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.imgui` | Dear ImGui windows for debug panels and tools. | [`lua-api/imgui.md`](../lua-api/imgui.md) |
| `haylen.ui` | The 62 component kinds of retained GUIs with their properties and events, layout and anchors, glass surfaces, themes and styles, focus and navigation, accessibility, long collections, screens with their stacks, tabs, splits, presentations, messages and routes, and the types `Gui`, `UiCollection`, `UiCell`, `UiTransform`, `NavigationStack`, `NavigationTabs`, `NavigationSplit`, `Presentation`, `Message` and `Router`. | [`lua-api/ui.md`](../lua-api/ui.md) |
