# Marketplace

Marketplace is an app sample of a local market of second-hand goods in a made-up city: twelve thousand listings in a grid and as price pins on a map of the city that the engine draws, listings with galleries and a map of their pickup area, offers, sellers with their reviews, an inbox of chats and a form to sell an item. It looks like a product of its kind in a fresh teal, with a light and a dark theme, and it works with a mouse, touch, a keyboard, gamepads and TV remotes on phones, tablets, desktops and televisions. Every listing, seller, review and message is fake data made from a seed, so the same market shows on every run.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/apps/marketplace` |
| macOS app | `haylen run samples/apps/marketplace --platform macos` |
| iPhone and iPad simulator | `haylen run samples/apps/marketplace --platform ios-simulator` |
| Apple TV simulator | `haylen run samples/apps/marketplace --platform tvos-simulator` |
| Android device or emulator | `haylen run samples/apps/marketplace --platform android --device <serial>` |
| Browser | `haylen run samples/apps/marketplace --platform web` |
| Headless player | `build/macos-debug/bin/haylen-headless samples/apps/marketplace --frames 6000` |

On the headless platform the app runs a tour by itself: it filters the listings, browses the map, opens a listing, makes an offer, reads the seller and the inbox, chats, publishes an item after a failed check of the form, logs every step and quits.

## Screens

| Screen | What it shows |
| --- | --- |
| Browse | Every listing in a recycled grid that fits its columns to the width, narrowed by a typed text, the category chips and a dialog with a price range and the condition, sorted by date, price or views. The map view shows the same listings as price pins on the city, with a strip of the listings around its center over it. |
| Listing | A gallery of three pictures in a carousel, the price, the condition, the description, the seller, the pickup area on a map and the buttons that message the seller or make an offer in a dialog. |
| Seller | The rating, the sales and the reply time, and the listings or the reviews of the seller in one recycled collection. |
| Inbox | The conversations, the newest on top, with the unread messages. |
| Chat | The listing and the messages in a recycled list that sticks to its end. The seller answers a moment after every message. |
| Sell | A form with the photo of the item and its backdrop, the title, the category, the condition, the price and a description, which marks every field that is missing before it publishes. |
| Saved | The listings saved with the heart. |
| You | The person as a seller, the dark theme and the listings the person published. |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Move between controls, cells and tabs | The arrows and Tab, or the pointer | The directional pad and the left stick | A tap | The directions |
| Press | Enter, Space or a click | The south button | A tap | Select or a click |
| Go back | Escape or the back button | The east button | The back button | Back or Menu |
| Pan the map | A drag, or the arrows while the map has the focus | The left stick or the directional pad while the map has the focus | A drag | The directions while the map has the focus |
| Zoom the map | The wheel, or plus and minus | The triggers | A pinch | Not available, the strip of listings follows the focus |
| Open the listing in the middle of the map | A click on its pin, or Enter | The south button | A tap on its pin | Select |

## How it works

The screens, the shell with its tab bar and rail, the background and the navigation work as in the [food delivery sample](../food-delivery/README.md), with the helper `Shell.centered` that keeps a column at most as wide as a reading width in the middle of wide windows.

- The grid of the browse tab is a `collection` with `cellAspect`, so its cells need no measuring, and with `minCellSize`, so it shows two columns on a phone and five on a desktop. A filter builds the new list from the twelve thousand listings and gives it to the collection, which compares the items by id.
- The map of the browse tab is a `playArea`. While it has the focus, the arrows, the directional pad and the left stick reach the actions `mapPan`, `mapZoom` and `mapOpen` of the action map instead of moving the focus, so a remote pans the map like a game world, and Tab or the View button go back to the controls. The scene draws the city and the pins in a world canvas whose camera viewport is the rectangle of the play area. A pin claims the cells of a coarse grid it covers, so pins never pile up, and the hidden ones appear as the map zooms in.
- The pickup map of a listing is drawn into a render target every frame, which `ui.setImage` names, so an `image` node of the page shows it with rounded corners inside the scroll, under the transforms of the transition.
- The chats, in `source/chats.lua`, and the saved listings, in `source/saved.lua`, emit `chatChanged` and `savedChanged` on [`haylen.events`](../../../docs/lua-api/events.md), and the screens listen with themselves as owners. The answer of a seller arrives through a timer owned by the chats, so it reaches the inbox after the chat closed.
- The sell form keeps its values from the events of its fields and marks each missing one with the `error` of its `formField`.

## Art

The art follows the style guide of Tiny Island, the standard of the repository: chunky cute shapes, a bold navy outline `#1E1B33`, soft cel shading with two tones and one highlight, light from the top left and bright saturated colors.

| Part | Rule |
| --- | --- |
| Goods | `content/goods/`: eighteen SVG illustrations of 200 by 200 units on a transparent background, one good in a three-quarter view with a soft shadow, drawn over a backdrop. |
| Backdrops | `content/goods/backdrops/`: eight pastel squares with two soft white circles, so one good shows on several backgrounds without an image for each listing. |
| Icons | `content/icons/`: line icons of 24 units with strokes of 2 in `currentColor`, which the buttons and `ui.icon` tint. |
| Logo | `content/images/logo.svg`: a little market stall with a price tag on a teal tile with the navy outline. |
| Illustration | `content/images/hero.png`: the one painted image of the app, a moving box full of second-hand treasures, generated with the `codex` command line and its `$imagegen` skill in 1024 by 768 pixels with a transparent background and scaled to 768 by 576. It shows at the top of the sell form and in the empty saved tab. |
| Palette | Mint white `#F4F7F6` and white surfaces, text `#14211D` and `#62726C`, teal `#0E9384` as the accent, and the dark theme on `#101816`. |
| Font | Prompt, in its regular and semibold faces, under the SIL Open Font License in `content/fonts/prompt-license.txt`. |

## Package layout

```text
marketplace/
  app.json               Window, physical UI scale, any orientation and the identifier.
  source/
    main.lua             Registers the font and the light and dark themes, and starts the shell.
    shell.lua            Tabs, navigation, notices, the rail and the tab bar, the header and centered columns.
    page.lua             The base of every screen.
    widgets.lua          The picture of a listing and the templates of the cells of listings, conversations and reviews.
    chats.lua            The conversations and the answers of the sellers.
    saved.lua            The saved listings.
    city.lua             The city map: its blocks and its drawing.
    random.lua           The seeded generator of the fake data.
    tour.lua             The automatic run of the headless platform.
    data/market.lua      The categories, the listings, the sellers, their reviews and the person.
    screens/             Browse, listing, seller, inbox, chat, sell, saved and profile, which quits the app where quitting closes it.
  content/
    goods/               The goods and their backdrops.
    icons/               The line icons.
    images/              The logo and the illustration.
    fonts/               Prompt and its license.
```
