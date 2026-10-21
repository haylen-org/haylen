# Online Store

Online Store is an app sample of a shop of clothes, shoes, bags and things for the home: three thousand products in sixteen categories, the edit of the season, product pages with a gallery in every color, sizes and reviews, a bag with promo codes, the checkout with a form that checks the address, orders that move from the warehouse to the door and saved products. It looks like a product of its kind in a bright and airy look of warm paper, ink and a serif, and it works with a mouse, touch, a keyboard, gamepads and TV remotes on phones, tablets, desktops and televisions. Every product, review and order is fake data made from a seed, so the same store shows on every run.

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
| Headless player | `build/macos-debug/bin/haylen-headless samples/apps/online-store` |

On the headless platform the app runs a tour by itself: it browses a category with filters, opens a product, picks a color and a size, fills the bag, applies a promo code, checks out once without a street and once with it, follows the order through its stages, saves products and opens the account, logs every step and quits.

## Screens

| Screen | What it shows |
| --- | --- |
| Shop | The edit of the season with the illustration of the app, the categories as tiles, the promises of the store and shelves of new arrivals, best sellers and offers. |
| Browse | Every product in a recycled grid that fits its columns to the width, narrowed by a typed text, the category chips and a dialog with the price range, the colors and the sale, sorted by popularity, novelty, price or rating. |
| Product | A gallery of four pictures in the chosen color, the name and the price in the serif with the old price struck through, the rating with the reviews in a dialog, the colors as round swatches, the sizes, the quantity, the details in an accordion and related products. The gallery stands beside the details on wide windows and above them on phones, and adding opens a dialog that leads to the bag. |
| Bag | The products with steppers for their quantities, a promo code and the costs, in a card beside the products on wide windows. |
| Checkout | The shipping address, which marks every empty field, the delivery, the payment and the summary with the button that places the order. |
| Confirmation | The thanks, the number of the order and the way to follow it. |
| Orders | The orders with their state, the newest on top. |
| Order | The state of the order, the stages from the order to the door on a line of steps that fills as it moves on, the products and the total. |
| Saved | The products kept with the heart. |
| Account | The person with the counts of orders and saved products, the notices, the address, help and, where quitting closes the app, a button that quits it. |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Move between controls, cells and tabs | The arrows and Tab, or the pointer | The directional pad and the left stick | A tap | The directions |
| Press | Enter, Space or a click | The south button | A tap | Select or a click |
| Go back | Escape or the back button | The east button | The back button | Back or Menu |
| Turn the gallery | The arrows on its dots, or its arrow buttons | The directional pad on its dots | A swipe | The directions on its dots |

## How it works

The screens are scenes of `source/screens/` built on the base of `source/page.lua`, which mounts their GUI with the scene as owner, paints the paper of the store and tells the screen when the window crosses the wide width. The shell of `source/shell.lua` switches the tabs as the only screen without a transition, pushes the other screens with a slide, puts the order in the place of its confirmation with `Shell.replace` and goes back with the back button, Escape and the east button. A phone shows the bar of tabs with their names at the bottom, and a wide window shows the wordmark, the tabs, the search and the bag across the top, the way a store on the web does, with the tab that shows underlined.

- One illustration serves every color of a product. The goods are drawn in white with a navy outline and navy shading, and the `tint` of their `image` gives them the color of the product, which the templates bind from the `tint` field of the item and the product page changes with `gui:set` when the person picks a color.
- The swatches of the colors are round SVG documents made at run time for every color, plain and inside a dark ring, named with `ui.setImage`, and the product page gives the swatch it picks the ring.
- The price of a product on sale is a `richText` with the price in the color of the sale and the old price inside `[s]`, so the strike follows the text at every size.
- The grid of the browse tab is a `collection` with `cellAspect` and `minCellSize`, so it shows two columns on a phone and five on a desktop, and the picture of a card takes the room its texts leave in the cell. A filter builds the new list and gives it to the collection, which compares the items by id.
- The bag, in `source/bag.lua`, the saved products, in `source/wishlist.lua`, and the orders, in the autoload `source/orders.lua`, emit `bagChanged`, `wishlistChanged` and `orderChanged` on [`haylen.events`](../../../docs/lua-api/events.md). An order goes through its stages on every screen, and the bag buttons of every screen follow the count of the bag.
- The checkout keeps the values of its fields from their events, so a window that crosses the wide width lays the form out again without losing them, and marks each empty field with the `error` of its `formField`.

## Style guide

| Part | Rule |
| --- | --- |
| Palette | Warm paper `#FAF7F2` with white `#FFFFFF` surfaces and sand `#F2EBE1` for the edit and the summaries, ink `#1D1B18` for the text and the primary buttons, warm gray `#776F66` for the quiet text, lines `#E9E2D8`, forest green `#2D6A4F` for links, the focus and the steps an order reached, terracotta `#C2452D` for the sales and gold `#E0A526` for the stars. The colors of the products are coral, navy, sage, sand, sky, plum, charcoal, mustard and white. |
| Type scale | Crimson Text, a serif, in bold for the titles, the headings and the prices, and Hind for everything else. Captions 13.5 points, body 16.5, labels 15.5 in bold, headings 22, titles 30 and the edit 40, in points of the em square, which `source/style.lua` turns into the heights the theme takes. |
| Spacing scale | Steps of 4, 8, 12, 16, 24, 32, 48 and 64 UI units. Pages keep 24 at their sides on a phone and 32 on a wide window, sections keep 24 above their titles, and the page holds at most 1200 units, or 760 for reading. |
| Shapes | Soft rectangles: controls with corners of 10, chips of 8, pictures of 14 and cards of 16, with thin lines of 1.5 and no shadows on the page. |
| Sizes | Controls 60 tall, the bar of tabs 92 tall, the navigation of the top 96 tall, cards of the shelves 248 wide with pictures a little taller than wide, and category tiles of 148. |
| Products | `content/products/`: sixteen SVG illustrations of 200 by 200 units on a transparent background, one product drawn in white with a navy outline, navy shading at 16 percent and a soft shadow, so the `tint` of the picture paints it in each of its colors. |
| Backdrops | `content/products/backdrops/`: eight pastel squares with two soft white circles behind the products. |
| Icons | `content/icons/`: thin line icons of 24 units with strokes of 1.6 in `currentColor`, which the buttons, `ui.icon` and the items of lists tint. |
| Logo and marks | `content/images/logo.svg`: a cream shopping bag with a terracotta tag on a forest green tile with the navy outline, and the steps of an order, empty and reached. |
| Illustration | `content/images/hero.png`: the one painted image of the app, shopping bags with a sneaker, a watch, a striped shirt and a gift, generated with the `codex` command line and its `$imagegen` skill in 1024 by 768 pixels with a transparent background and scaled to 768 by 576. It shows on the edit, the confirmation and the empty states. |

## Package layout

```text
online-store/
  app.json               Window, physical UI scale, any orientation, the orders autoload and the identifier.
  source/
    main.lua             Registers the theme and the swatches, and starts the shell.
    style.lua            The palette, the type scale, the spacing scale, the shapes and the theme.
    shell.lua            Tabs, navigation, notices, the navigation of the top, the bar of tabs, the bag buttons and the headers.
    page.lua             The base of every screen.
    widgets.lua          The picture of a product, the product card, the shelves, the swatches and the rows of costs.
    bag.lua              The bag and its promo codes.
    wishlist.lua         The saved products.
    orders.lua           The orders and their stages, an autoload.
    random.lua           The seeded generator of the fake data.
    tour.lua             The automatic run of the headless platform.
    data/store.lua       The colors, the categories, the products and their reviews.
    screens/             Shop, browse, product, bag, checkout, confirmation, orders, order, saved and account.
  content/
    products/            The products and their backdrops.
    icons/               The line icons.
    images/              The logo, the steps and the illustration.
    fonts/               Hind, Crimson Text and their licenses.
```
