# Online Store

Online Store is an app sample of a shop of clothes, shoes, bags and things for the home: three thousand products in sixteen categories, product pages with a gallery in every color, sizes, reviews, a bag with promo codes, the checkout with a form that checks the address, and orders followed on a map of a made-up city that the engine draws. It looks like a product of its kind in coral and navy, with a light and a dark theme, and it works with a mouse, touch, a keyboard, gamepads and TV remotes on phones, tablets, desktops and televisions. Every product, review and order is fake data made from a seed, so the same store shows on every run.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/apps/online-store` |
| macOS app | `haylen run samples/apps/online-store --platform macos` |
| iPhone and iPad simulator | `haylen run samples/apps/online-store --platform ios-simulator` |
| Apple TV simulator | `haylen run samples/apps/online-store --platform tvos-simulator` |
| Android device or emulator | `haylen run samples/apps/online-store --platform android --device <serial>` |
| Browser | `haylen run samples/apps/online-store --platform web` |
| Headless player | `build/macos-debug/bin/haylen-headless samples/apps/online-store --frames 6000` |

On the headless platform the app runs a tour by itself: it browses a category with filters, opens a product, picks a color and a size, fills the bag, applies a promo code, checks out once without a street and once with it, follows the order on the map, saves products and switches the theme, logs every step and quits.

## Screens

| Screen | What it shows |
| --- | --- |
| Shop | The campaign of the season, the categories and three horizontal shelves of products. |
| Browse | Every product in a recycled grid that fits its columns to the width, narrowed by a typed text, the category chips and a sheet with the price range, the colors and the sale, sorted by popularity, price or rating. |
| Product | A gallery of four pictures in the chosen color, the price with the old price struck through, the rating with the reviews in a sheet, the colors, the sizes, the quantity, the details in an accordion and related products. Adding opens a sheet that leads to the bag. |
| Bag | The products with steppers for their quantities, a promo code and the costs, beside the products on wide windows. |
| Checkout | The shipping address, which marks every empty field, the delivery, the payment and the total. |
| Confirmation | The order number and the way to follow it. |
| Orders | The orders with their state. |
| Order | The van on a map of the city drawn into a render target that the page shows as a picture, the stages of the delivery and the products. |
| Wishlist | The products saved with the heart. |
| Account | The profile, the dark theme, the notices, the address and help. |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Move between controls, cells and tabs | The arrows and Tab, or the pointer | The directional pad and the left stick | A tap | The directions |
| Press | Enter, Space or a click | The south button | A tap | Select or a click |
| Go back | Escape or the back button | The east button | The back button | Back or Menu |
| Turn the gallery | The arrows on its dots, or its arrow buttons | The directional pad on its dots | A swipe | The directions on its dots |

## How it works

The screens, the shell with its tab bar and rail, the background and the navigation work as in the [food delivery sample](../food-delivery/README.md), with `Shell.centered` for reading widths and `Shell.replace` for the confirmation.

- One illustration serves every color of a product. The goods are drawn in white with a navy outline and navy shading, and the `tint` of their `image` gives them the color of the product, which the templates bind from the `tint` field of the item and the product page changes with `gui:set` when the person picks a color.
- The price of a product on sale is a `richText` with the old price inside `[s]`, so the strike follows the text at every size.
- The grid of the browse tab is a `collection` with `cellAspect` and `minCellSize`, so it shows two columns on a phone and six on a desktop, and a filter builds the new list and gives it to the collection, which compares the items by id.
- The top of the product page is a row with `wrap = true` whose two columns have a least and a greatest width, so they stand side by side on wide windows and one above the other on phones.
- The bag, in `source/bag.lua`, the wishlist, in `source/wishlist.lua`, and the orders, in the autoload `source/orders.lua`, emit `bagChanged`, `wishlistChanged` and `orderChanged` on [`haylen.events`](../../../docs/lua-api/events.md). An order goes through its stages on every screen, and the van drives the shortest route from the depot to the door, which the order page draws into its render target every frame.
- The checkout keeps the values of its fields from their events and marks each empty one with the `error` of its `formField`.

## Art

The art follows the style guide of Tiny Island, the standard of the repository: chunky cute shapes, a bold navy outline `#1E1B33`, soft cel shading and one highlight, light from the top left and bright saturated colors.

| Part | Rule |
| --- | --- |
| Products | `content/products/`: sixteen SVG illustrations of 200 by 200 units on a transparent background, one product drawn in white with a navy outline, navy shading at 16 percent and a soft shadow, so the `tint` of the picture paints it in each of its colors. |
| Backdrops | `content/products/backdrops/`: eight pastel squares with two soft white circles behind the products. |
| Icons | `content/icons/`: line icons of 24 units with strokes of 2 in `currentColor`, which the buttons and `ui.icon` tint. |
| Logo | `content/images/logo.svg`: a white shopping bag with a navy check on a coral tile with the navy outline. |
| Illustration | `content/images/hero.png`: the one painted image of the app, shopping bags with a sneaker, a watch, a striped shirt and a gift, generated with the `codex` command line and its `$imagegen` skill in 1024 by 768 pixels with a transparent background and scaled to 768 by 576. It shows on the campaign, the confirmation and the empty states. |
| Palette | Warm white `#FBF8F4` and white surfaces, navy text `#1F2A44` and `#6E7488`, coral `#F2545B` as the accent, and the dark theme on `#14151C`. The colors of the products are coral, navy, sage, sand, sky, plum, charcoal, mustard and white. |
| Font | Hind, in its regular and bold faces, under the SIL Open Font License in `content/fonts/hind-license.txt`. |

## Package layout

```text
online-store/
  app.json               Window, physical UI scale, any orientation, the orders autoload and the identifier.
  source/
    main.lua             Registers the font and the light and dark themes, and starts the shell.
    shell.lua            Tabs, navigation, notices, the rail and the tab bar, the header and centered columns.
    page.lua             The base of every screen.
    widgets.lua          The bag button, the picture of a product and the template of the product card.
    bag.lua              The bag and its promo codes.
    wishlist.lua         The saved products.
    orders.lua           The orders and their stages, an autoload.
    city.lua             The city map: its blocks, its routes and its drawing.
    random.lua           The seeded generator of the fake data.
    tour.lua             The automatic run of the headless platform.
    data/store.lua       The colors, the categories, the products and their reviews.
    screens/             Shop, browse, product, bag, checkout, confirmation, orders, order, wishlist and account.
  content/
    products/            The products and their backdrops.
    icons/               The line icons.
    images/              The logo and the illustration.
    fonts/               Hind and its license.
```
