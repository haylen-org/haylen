# Food Delivery

Food Delivery is an app sample of a food delivery service in a made-up city: 2,400 restaurants with their menus, a cart, the checkout and the order on a map of the city that the engine draws, with the courier riding the route to the door. It looks like a product of its kind in a warm tomato red, with a light and a dark theme, and it works with a mouse, touch, a keyboard, gamepads and TV remotes on phones, tablets, desktops and televisions. Every restaurant, dish, courier and order is fake data made from a seed, so the same city shows on every run.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/apps/food-delivery` |
| macOS app | `haylen run samples/apps/food-delivery --platform macos` |
| iPhone and iPad simulator | `haylen run samples/apps/food-delivery --platform ios-simulator` |
| Apple TV simulator | `haylen run samples/apps/food-delivery --platform tvos-simulator` |
| Android device or emulator | `haylen run samples/apps/food-delivery --platform android --device <serial>` |
| Browser | `haylen run samples/apps/food-delivery --platform web` |
| Headless player | `build/macos-debug/bin/haylen-headless samples/apps/food-delivery --frames 4000` |

On the headless platform the app runs a tour by itself: it searches, opens a restaurant, picks a dish with its options, fills the cart, checks out, follows the order on the map in both themes, logs every step and quits.

## Screens

| Screen | What it shows |
| --- | --- |
| Home | The delivery address, the search bar, the offer of the week, the cuisines and two shelves of restaurants in horizontal recycled lists. |
| Search | Every restaurant in one recycled list that a typed text, a cuisine and the quick filters narrow, sorted by rating, time, fee or distance. |
| Restaurant | The cover, the menu in sections with sticky headers and shortcuts to each section, the sheet of a dish with its size, extras and quantity, and the bar that leads to the cart. |
| Cart | The dishes with steppers for their quantities and the costs, beside the dishes on wide windows. |
| Checkout | The address, the delivery speed, the payment, the tip, a note for the courier and the total. |
| Tracking | The map of the city with the route and the courier, under a sheet with the stage, the minutes left, the courier and the order, which moves to the side on wide windows. |
| Orders | The orders on their way, which open the tracker, and the past orders, which open their restaurant again. |
| Account | The profile, the dark theme, the notices, the saved addresses, the ways to pay and, where quitting closes the app, a button that quits it. |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Move between controls, cells and tabs | The arrows and Tab, or the pointer | The directional pad and the left stick | A tap | The directions |
| Press | Enter, Space or a click | The south button | A tap | Select or a click |
| Go back | Escape or the back button | The east button | The back button | Back or Menu |
| Scroll a list | The wheel, Page Up, Page Down, Home and End | The focus and the shoulders | A drag or a fling | The focus |
| Pan and zoom the map | A drag and the wheel | The button that shows the whole route | A drag and a pinch | The button that shows the whole route |

## How it works

Every screen is a scene of [`haylen.scene`](../../../docs/lua-api/scene.md) built on `source/page.lua`, which paints the background of the theme and mounts the GUI of the screen with the scene as its owner. The module `source/shell.lua` holds the navigation: the tabs replace the root screen without a transition, the way native tab bars switch, other screens slide in and out with `slideIn`, and Escape, the east button and the back buttons of TVs and phones go back through the `onCancel` handler of every screen root.

- The interface keeps its size in points on every screen with the physical scale of [`haylen.ui`](../../../docs/lua-api/ui.md#uisetscalemodemode), and a television draws it half as large again. Phones show a tab bar at the bottom, and windows wider than 960 units show a rail on the side instead, which `Shell.adapt` switches as the window changes. The cart and the tracker move their summary and sheet to the side on wide windows.
- The long lists are `collection` nodes with recycled cells: the 2,400 restaurants of the search tab in a grid that fits its columns to the width, the menu of a restaurant with sticky section headers, the shelves of the home screen along the horizontal axis, the cart with a stepper bound to every quantity, and the orders.
- The cart, in `source/basket.lua`, and the orders, in the autoload `source/orders.lua`, emit `basketChanged` and `orderChanged` on [`haylen.events`](../../../docs/lua-api/events.md), and the screens listen with themselves as owners. The orders move through their stages on every screen, and every stage shows a notice in the toast stack that stays above the screens.
- The city of `source/city.lua` is a grid of blocks with streets, avenues, a river crossed only on the bridges of the avenues, parks and buildings, made from a seed. The route of an order is the shortest path over its corners, found with A*, and the tracker draws the city, the route and the courier with [`haylen.graphics2d`](../../../docs/lua-api/graphics2d.md) in a world canvas behind the GUI, fitted to the part of the screen the sheet leaves.
- The dark theme of the account tab switches the theme of the UI with `ui.setTheme` and keeps the choice in [`haylen.preferences`](../../../docs/lua-api/preferences.md), and the map takes a dark palette with it.

## Art

The art follows the style guide of Tiny Island, the standard of the repository: chunky cute shapes, a bold navy outline `#1E1B33`, soft cel shading with two tones and one highlight, light from the top left and bright saturated colors.

| Part | Rule |
| --- | --- |
| Dishes | `content/dishes/`: eighteen SVG illustrations of 320 by 200 units, one dish in a three-quarter view on a pastel card with two soft white circles, shown with `fit = 'cover'` from the wide cards to the round tiles of the cuisines. |
| Icons | `content/icons/`: line icons of 24 units with strokes of 2 in `currentColor`, which the buttons, `ui.icon` and the items of lists and menus tint. |
| Logo | `content/images/logo.svg`: a white bowl with steam on a tomato red tile with the navy outline. |
| Offer | `content/images/hero.png`: the one painted image of the app, a red delivery scooter among a burger, pizza, ramen and fries, generated with the `codex` command line and its `$imagegen` skill in 1024 by 768 pixels with a transparent background and scaled to 768 by 576. |
| Palette | Cream `#FFF8F1` and white surfaces, text `#2A1E1A` and `#8A766B`, tomato red `#E8452C` as the accent, and the dark theme on `#1C1512`. |
| Font | Poppins, in its regular and semibold faces, under the SIL Open Font License in `content/fonts/poppins-license.txt`. |

## Package layout

```text
food-delivery/
  app.json               Window, physical UI scale, any orientation, the orders autoload and the identifier.
  source/
    main.lua             Registers the font and the light and dark themes, and starts the shell.
    shell.lua            Tabs, navigation, notices, the rail and the tab bar, and the header of the screens.
    page.lua             The base of every screen.
    widgets.lua          The cart button and the templates of the cells of restaurants, dishes and orders.
    basket.lua           The cart.
    orders.lua           The orders and their stages, an autoload.
    city.lua             The city map: its blocks, its routes and its drawing.
    random.lua           The seeded generator of the fake data.
    tour.lua             The automatic run of the headless platform.
    data/catalog.lua     The cuisines, the restaurants and their menus.
    data/account.lua     The profile, the addresses and the ways to pay.
    screens/             Home, search, restaurant, cart, checkout, tracking, orders and account.
  content/
    dishes/              The dish illustrations.
    icons/               The line icons.
    images/              The logo and the offer illustration.
    fonts/               Poppins and its license.
```
