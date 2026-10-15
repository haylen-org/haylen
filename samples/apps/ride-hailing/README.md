# Ride Hailing

Ride Hailing is an app sample of a ride service in a made-up city at night: a map of the city that the engine draws with live traffic, the search among 565 places, six kinds of ride with their prices, the whole trip from the search for a driver to the rating, 520 past trips, a wallet and an account. It looks like a product of its kind in a night look with lime highlights and a day theme, and it works with a mouse, touch, a keyboard, gamepads and TV remotes on phones, tablets, desktops and televisions. Every place, driver and trip is fake data made from a seed, so the same city shows on every run.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/apps/ride-hailing` |
| macOS app | `haylen run samples/apps/ride-hailing --platform macos` |
| iPhone and iPad simulator | `haylen run samples/apps/ride-hailing --platform ios-simulator` |
| Apple TV simulator | `haylen run samples/apps/ride-hailing --platform tvos-simulator` |
| Android device or emulator | `haylen run samples/apps/ride-hailing --platform android --device <serial>` |
| Browser | `haylen run samples/apps/ride-hailing --platform web` |
| Headless player | `build/macos-debug/bin/haylen-headless samples/apps/ride-hailing --frames 6000` |

On the headless platform the app runs a tour by itself: it searches a destination, compares the kinds of ride, requests one, follows the trip through the search, the driver on the way, the chat, the safety tools and the ride, rates it with a tip, opens the activity, a past trip, the wallet and the account in both themes, logs every step and quits.

## Screens

| Screen | What it shows |
| --- | --- |
| Ride | The map with the traffic and the rider, under a sheet that asks where to, with the saved places, the recent destinations and the ride in progress. |
| Search | The pickup and a field that narrows the 565 places of the city in a recycled list as the person types, with the saved places first. |
| Route | The route on the map with the pickup and the destination, the six kinds of ride with their prices and times, the payment and the request. |
| Trip | The radar while the app looks for a driver, the car of the driver coming to the pickup, waiting and riding the route, under a sheet with the stage, the minutes left, the driver and the chat, safety and cancel dialogs, and the rating with a tip at the end. |
| Activity | The 520 past trips in a recycled list under sticky months. |
| Trip details | The route on a map drawn into a render target that the page shows as a picture, the places, the driver and the receipt. |
| Wallet | The balance with a dialog that adds funds, the ways to pay and the movements. |
| Account | An invitation, the day theme, the safety options and the trusted contacts. |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Move between controls, cells and tabs | The arrows and Tab, or the pointer | The directional pad and the left stick | A tap | The directions |
| Press | Enter, Space or a click | The south button | A tap | Select or a click |
| Go back | Escape or the back button | The east button | The back button | Back or Menu |
| Pan and zoom the map | A drag and the wheel | The button that shows the whole route | A drag and a pinch | The button that shows the whole route |

## How it works

The screens, the shell with its tab bar and rail, the background and the navigation work as in the [food delivery sample](../food-delivery/README.md), and `Shell.replace` puts the route in the place of the search and the trip in the place of the route, so going back from a trip returns to the map.

- The autoload `source/rides.lua` holds the trip in progress and moves it through its stages on every screen: five seconds of search, the driver riding the route from where they are to the pickup, five seconds at the pickup and the ride to the destination, with the times of the routes from their lengths. Every change emits `tripChanged` on [`haylen.events`](../../../docs/lua-api/events.md), which the ride tab, the trip and the activity hear, and shows a notice.
- The traffic of `source/traffic.lua` is 36 cars that ride the shortest routes between random corners of the city and pick a new route when they arrive. The autoload advances it, so the map is alive behind every screen.
- The module `source/map-view.lua` fits a part of the city into the room the GUI leaves beside its sheet, follows drags, the wheel and pinches, and draws the city, the cars seen from above, routes with an outline, pins with labels, the pulse of the rider and the radar of the search with [`haylen.graphics2d`](../../../docs/lua-api/graphics2d.md) in a world canvas behind the GUI. Every mark keeps its size on the screen at every zoom.
- The kinds of ride are a `collection` with single selection that follows the focus, so a remote picks a kind by moving to it, and the request button names it.
- The map of a past trip is drawn into a render target every frame, which `ui.setImage` names, so an `image` node shows it with rounded corners inside the scroll of the page.

## Art

The art follows the style guide of Tiny Island, the standard of the repository: chunky cute shapes, a bold navy outline `#1E1B33`, soft cel shading with two tones and one highlight, light from the top left and bright saturated colors.

| Part | Rule |
| --- | --- |
| Rides | `content/rides/`: six SVG illustrations of 240 by 140 units on a transparent background, a vehicle of each kind seen from the side with a soft shadow. |
| Icons | `content/icons/`: line icons of 24 units with strokes of 2 in `currentColor`, which the buttons, `ui.icon` and the pins of the map tint. |
| Logo | `content/images/logo.svg`: a lime pin at the end of a dashed road on a night tile with the navy outline. |
| Illustration | `content/images/hero.png`: the one painted image of the app, a teal car with its headlights on a road toward a lime pin under the moon, generated with the `codex` command line and its `$imagegen` skill in 1024 by 768 pixels with a transparent background and scaled to 768 by 576. It shows on the invitation of the account tab. |
| Map | Drawn with primitives: night blocks and buildings, streets and avenues, a river with bridges, parks with trees, cars seen from above with their headlights and lime routes. |
| Palette | Night navy `#0B1020` and `#161D33` surfaces, text `#EAF0FF` and `#8C97B8`, lime `#B6F24A` as the accent with dark ink, and the day theme on `#F2F5FA` with a deeper green. |
| Font | Barlow, in its regular and semibold faces, under the SIL Open Font License in `content/fonts/barlow-license.txt`. |

## Package layout

```text
ride-hailing/
  app.json               Window, physical UI scale, any orientation, the rides autoload and the identifier.
  source/
    main.lua             Registers the font and the night and day themes, and starts the shell.
    shell.lua            Tabs, navigation, notices, the rail and the tab bar, the header and centered columns.
    page.lua             The base of every screen.
    rides.lua            The kinds of ride, the trip in progress, the past trips and the wallet, an autoload.
    traffic.lua          The cars of the city.
    map-view.lua         The camera and the drawing of the map.
    city.lua             The city map: its blocks, its routes and its drawing.
    random.lua           The seeded generator of the fake data.
    tour.lua             The automatic run of the headless platform.
    data/places.lua      The places of the city and the saved places.
    screens/             Ride, search, route, trip, activity, trip details, wallet and account.
  content/
    rides/               The vehicles of the kinds of ride.
    icons/               The line icons.
    images/              The logo and the illustration.
    fonts/               Barlow and its license.
```
