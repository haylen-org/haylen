# Ride Hailing

Ride Hailing is an app sample of a ride service in a made-up city: a day map of the city that fills the screen with live traffic, the search among 565 places, six kinds of ride with their prices and times, the whole trip from the search for a driver to the rating, 520 past trips, a wallet and an account behind a menu. It looks like a product of its kind in a sunny look of white sheets, ink and yellow over the map, and it works with a mouse, touch, a keyboard, gamepads and TV remotes on phones, tablets, desktops and televisions. Every place, driver and trip is fake data made from a seed, so the same city shows on every run.

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
| Headless player | `build/macos-debug/bin/haylen-headless samples/apps/ride-hailing` |

On the headless platform the app runs a tour by itself: it searches a destination, compares the kinds of ride, requests one, follows the trip through the search, the driver on the way, the messages, the safety tools and the ride, rates it with a tip, opens the menu, the trips, a past trip, the wallet and the account, logs every step and quits.

## Screens

| Screen | What it shows |
| --- | --- |
| Map | The map with the traffic and the rider, the round buttons of the menu and of the rider's place, and a sheet that asks where the rider is going, with the saved places, the recent destinations and the ride in progress. |
| Search | The pickup and the destination joined by a line, and a field that narrows the 565 places of the city in a recycled list as the person types, with the saved places first. |
| Ride | The route on the map with the pickup and the destination, and a sheet with the six kinds of ride with their prices and times, the payment and the request. |
| Trip | The radar while the app looks for a driver, the car of the driver coming to the pickup, waiting and riding the route, under a sheet with the stage, the minutes left in large numbers, the progress of the trip, the driver and the plate, the messages, the safety tools, the share and the cancel, and the rating with a tip at the end. |
| Menu | A panel that slides in from the left over the dimmed map with the rider, the trips, the wallet, the account and, where quitting closes the app, a button that quits it. |
| Trips | The 520 past trips in a recycled list under sticky months. |
| Trip details | The route on a map drawn into a render target that the page shows as a picture, the places, the driver and the receipt. |
| Wallet | The balance on a dark card with a dialog that adds funds, the ways to pay and the movements. |
| Account | The rider, an invitation with the illustration of the app, the safety options and the trusted contacts. |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Move between controls and cells | The arrows and Tab, or the pointer | The directional pad and the left stick | A tap | The directions |
| Press | Enter, Space or a click | The south button | A tap | Select or a click |
| Go back, close the menu | Escape or the back button | The east button | The back button or a tap beside the menu | Back or Menu |
| Pan and zoom the map | A drag and the wheel | The button that shows the whole route | A drag and a pinch | The button that shows the whole route |

## How it works

The screens are scenes of `source/screens/` built on the base of `source/page.lua`, which mounts their GUI with the scene as owner and tells the screen when the window crosses the wide width. The app has no tab bar: the map is the first screen, a round button opens the menu, which is a transparent scene over the map, and `Shell.openPage` opens a page of the menu over the map with the menu closed. Every step of a ride shows in the sheet of `Shell.sheet`, a white card that floats at the bottom of a phone with a handle, or as a panel at the top left of a wide window, and `Shell.replace` puts the ride in the place of the search and the trip in the place of the ride, so going back from a trip returns to the map.

- The autoload `source/rides.lua` holds the trip in progress and moves it through its stages on every screen: five seconds of search, the driver riding the route from where they are to the pickup, five seconds at the pickup and the ride to the destination, with the times of the routes from their lengths. Every change emits `tripChanged` on [`haylen.events`](../../../docs/lua-api/events.md), which the map, the trip and the trips hear, and shows a notice.
- The traffic of `source/traffic.lua` is 36 cars that ride the shortest routes between random corners of the city and pick a new route when they arrive. The autoload advances it, so the map is alive behind every screen.
- The module `source/map-view.lua` fits a part of the city into the room the sheet leaves, follows drags, the wheel and pinches, and draws the day map, the cars seen from above, the route with a white casing, the pickup and the destination with their labels, the pulse of the rider and the radar of the search with [`haylen.graphics2d`](../../../docs/lua-api/graphics2d.md) in a world canvas behind the GUI. Every mark keeps its size in UI units at every zoom, so it stays as large as the interface around it on every screen, and a block of the city never spans more than 110 points, so a short route never fills a large window with a few blocks.
- The kinds of ride are a `collection` with single selection that follows the focus, so a remote picks a kind by moving to it, and the request button names it with its price.
- The map of a past trip is drawn into a render target every frame, which `ui.setImage` names, so an `image` node shows it with rounded corners inside the scroll of the page, and its view keeps inside the city.

## Style guide

| Part | Rule |
| --- | --- |
| Palette | White `#FFFFFF` sheets with a soft gray `#F2F2EF` for fields and round actions, ink `#15161A` for the text, the route, the pins and the wallet card, gray `#686A72` for the quiet text, yellow `#FFC629` for the primary buttons, the car of the driver, the radar and the progress of a trip, and a blue `#2F6BFF` dot for the rider. |
| Map | Warm ground `#E9E6DC`, white streets with edges `#D9D4C8`, pale yellow avenues `#FFEBAE`, blocks `#F3F1EA` with buildings in four warm grays, green parks `#C6E6B2` with trees, a blue river `#A9D9EF` with white bridges, and the cars of the traffic in soft white, sky, coral, mint and lilac, outlined in ink. |
| Type scale | Barlow, in its regular and semibold faces. Captions 14 points, body 17, labels 17 in semibold, headings 24, titles 32 and the minutes left of a trip and the balance 48, in points of the em square, which `source/style.lua` turns into the heights the theme takes. |
| Spacing scale | Steps of 4, 8, 12, 16, 24, 32 and 48 UI units. Sheets keep 24 inside and float 8 from the edges of a phone or 24 from the corner of a wide window, and the buttons over the map keep 16 from the edges. |
| Shapes | Sheets and cards with large corners of 28, rounded square controls of 16, chips of 12, and round buttons over the map and round actions in the sheets. |
| Sizes | Controls 64 tall, the request 72 tall, buttons over the map of 64, the panel of a wide window 540 wide and pages at most 820 wide. |
| Rides | `content/rides/`: six SVG illustrations of 240 by 140 units on a transparent background, a vehicle of each kind seen from the side with a soft shadow, in the style of the repository with the navy outline `#1E1B33`. |
| Icons | `content/icons/`: bold geometric icons of 24 units with strokes of 2.4 in `currentColor`, which the buttons, `ui.icon` and the items of lists tint. |
| Logo | `content/images/logo.svg`: a dark pin at the end of a dashed road on a yellow tile with the navy outline. |
| Illustration | `content/images/hero.png`: the one painted image of the app, a little yellow car on a winding road toward a dark pin under the sun, generated with the `codex` command line and its `$imagegen` skill in 1024 by 768 pixels with a transparent background and scaled to 768 by 576. It shows on the invitation of the account. |

## Package layout

```text
ride-hailing/
  app.json               Window, physical UI scale, any orientation, the rides autoload and the identifier.
  source/
    main.lua             Registers the theme and starts the shell.
    style.lua            The palette of the screens and of the map, the type scale, the spacing scale, the shapes and the theme.
    shell.lua            Navigation, notices, the buttons over the map, the sheets and the headers.
    page.lua             The base of every screen.
    rides.lua            The kinds of ride, the trip in progress, the past trips and the wallet, an autoload.
    traffic.lua          The cars of the city.
    map-view.lua         The camera and the drawing of the map.
    city.lua             The city: its blocks, its routes and its drawing.
    random.lua           The seeded generator of the fake data.
    tour.lua             The automatic run of the headless platform.
    data/places.lua      The places of the city and the saved places.
    screens/             Map, search, ride, trip, menu, trips, trip details, wallet and account.
  content/
    rides/               The vehicles of the kinds of ride.
    icons/               The icons.
    images/              The logo and the illustration.
    fonts/               Barlow and its license.
```
