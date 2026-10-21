# Photo Social

Photo Social is an app sample of a photo sharing network: a feed of posts with pictures, likes, comments and captions, stories that play full screen, profiles with a grid of pictures and a follow button, a post with its comments, a new post with a picture, a crop, a filter and a caption, and direct messages with a real chat. It looks like a product of its kind in a bright white look with a sunset ring around the stories and a blue accent, and it works with a mouse, touch, a keyboard, gamepads and TV remotes on phones, tablets, desktops and televisions. Every person, post, comment and message is fake data made from a seed, so the same feed shows on every run.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/apps/photo-social` |
| macOS app | `haylen run samples/apps/photo-social --platform macos` |
| iPhone and iPad simulator | `haylen run samples/apps/photo-social --platform ios-simulator` |
| Apple TV simulator | `haylen run samples/apps/photo-social --platform tvos-simulator` |
| Android device or emulator | `haylen run samples/apps/photo-social --platform android --device <serial>` |
| Browser | `haylen run samples/apps/photo-social --platform web` |
| Headless player | `build/macos-debug/bin/haylen-headless samples/apps/photo-social` |

On the headless platform the app runs a tour by itself: it likes a post, watches and answers a story, comments on a post, follows a person, searches people, chats, makes a post with a filter and a crop, edits the profile, saves posts and sends one to a friend, logs every step and quits.

## Screens

| Screen | What it shows |
| --- | --- |
| Home | The stories in a row, with the person's own first, over the feed of the people the person follows. A phone shows the pictures from edge to edge under the wordmark, and a wide window shows the posts as cards in a column, with the person and suggestions of people to follow beside it when there is room. |
| Story | The pictures of a person, five seconds each, under bars that fill as they play, then the stories of the next person. A tap on the left third goes back and anywhere else goes on, a press holds the story, and the field at the bottom replies in the direct messages. A wide window shows the pictures in a tall frame between two arrows. |
| Post | The post and its comments over the field that adds one, or on a wide window the picture on a dark stage beside the author, the comments, the buttons and the field. |
| Profile | The face, the counts of posts, followers and following, the name, the bio, the buttons that follow and message, and the grid of pictures. The person's own profile edits its name and bio in a dialog, starts a post and shows the saved posts in a second section. |
| Explore | A grid of the pictures of everyone, and the people whose name or handle matches the typed text. |
| New post | Three steps: a picture of the library, its crop and its filter, each shown on the middle of the picture, and the caption and the place before it goes to the top of the feed. |
| Messages | The conversations, the newest on top, with a dot on those with messages not read yet, narrowed by a typed name. |
| Chat | The messages in a recycled list that sticks to its end, with the posts that travel in them as pictures, and the field that sends a message. The other person answers a moment later. |

Every post has a menu with the way to the profile, sending it to a friend in a dialog and saving it.

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Move between controls, cells and tabs | The arrows and Tab, or the pointer | The directional pad and the left stick | A tap | The directions |
| Press | Enter, Space or a click | The south button | A tap | Select or a click |
| Step through the stories | The left and right arrows, or a click on the picture | The directional pad and the left stick | A tap on the picture | The directions |
| Hold a story | A press on the picture | | A press on the picture | |
| Go back | Escape or the back button | The east button | The back button | Back or Menu |

## How it works

The screens are scenes of `source/screens/` built on the base of `source/page.lua`, which mounts their GUI with the scene as owner, paints the background and tells the screen when the window crosses the widths of the wide and roomy layouts. The shell of `source/shell.lua` switches the tabs as the only screen without a transition, pushes the other screens with a slide and goes back with the back button, Escape and the east button.

- The pictures of `source/photos.lua` are twelve painted files that every post, story and profile reuses. Each one shows whole or as a close crop around its subject with a filter, made on the task pool with `image:crop` and `image:applyAsync` of [`haylen.graphics`](../../../docs/lua-api/graphics.md) and named for the UI with `ui.setImage`, so twelve files give twenty-four looks. Until a look is ready its name shows the mean color of its picture, so nothing waits on the frame thread, and the textures keep mipmaps, so the small tiles of a grid stay smooth.
- The faces of `source/avatars.lua` are SVG documents drawn from a few shapes, a round face with dots for eyes, rosy cheeks and a smile under one of six haircuts, read with `graphics.newVectorImage`, so every person has a face of a few hundred bytes that stays sharp at every size.
- The feed, the comments, the grids and the conversations are `collection` nodes. The posts of a phone and of a wide window are two templates of the same items, the heart over a liked picture is a part whose [transform](../../../docs/lua-api/ui.md#guitransformid) a tween fades and grows, and the head of a profile is the first cell of its grid, across all three columns.
- The story is a [play area](../../../docs/lua-api/ui.md#uiplayareaproperties) over the picture, so the arrows, the directional pad and a remote reach the actions `storyPrevious` and `storyNext` of the action map while the reply field and the buttons stay a Tab away. A press on the picture reports `press` and `release`, which hold the story and step through it.
- The new post decodes the chosen picture on the task pool, makes the small previews of every filter from its middle and the preview of the post with the crop and the filter, all in tasks that the screen owns.
- The data of `source/data/social.lua` and the conversations of `source/chats.lua` emit `socialChanged` and `chatChanged` on [`haylen.events`](../../../docs/lua-api/events.md), and the screens listen with themselves as owners. The answer of a friend arrives through a timer owned by the conversations, so it reaches the messages after the chat closed.
- The field of a chat stays above the on-screen keyboard, and a tap on the send button while the keyboard is open sends at once, because the GUIs hold their place while a press is down.

## Style guide

| Part | Rule |
| --- | --- |
| Palette | Paper white `#FFFFFF` with a soft gray `#F4F4F7` for incoming bubbles, ink `#141418` and gray `#71717E` for the text, hairlines `#EBEBF0`, blue `#2F6BFF` for the buttons, the focus and the bubbles of the person, red `#FF3358` for a liked heart and the sunset gradient of amber `#FFB547`, pink `#FF4F8B` and violet `#8A5CFF` for the ring of the unseen stories and the logo. |
| Type scale | Poppins, in its regular and semibold faces. Captions 13.5 points, body 16, labels 15.5 in semibold, headings 20, titles 26 and the wordmark 32, in points of the em square, which `source/style.lua` turns into the heights the theme takes. |
| Spacing scale | Steps of 4, 8, 12, 16, 24, 32 and 48 UI units. Rows of a post keep 16 at their sides on a phone and 24 in a card, and the feed of a wide window keeps 24 between its cards. |
| Shapes | Pictures run from edge to edge on a phone and take corners of 16 in the cards of a wide window. Buttons are rounded squares of 14, the fields and the bubbles round to 26, faces are circles, and the bar of a phone shows icons alone, filled while they show. |
| Sizes | Faces of 56 in posts and 44 in comments, stories of 112, controls 60 tall, the bar of icons 84 tall, the rail of a wide window 300 wide and the feed of a wide window at most 640 wide. |
| Pictures | `content/photos/`: twelve photo-like illustrations of 768 by 768 pixels painted for the app in the style of the repository, landscapes, food, pets and the city, with a clean dark navy outline `#1E1B33`, soft cel shading with two tones and one highlight, light from the top left and bright colors, generated with the `codex` command line and its `$imagegen` skill and saved as JPG files. |
| Faces | Drawn by `source/avatars.lua` with the same navy outline, in six skin tones, seven hair colors and eight shirts on pastel backgrounds. |
| Icons | `content/icons/`: line icons of 24 units with strokes of 2 in `currentColor`, with bolder or filled versions for the tab that shows, which the buttons and `ui.icon` tint. |
| Logo and marks | `content/images/`: the logo, a white camera on a tile of the sunset gradient with the navy outline, the rings of the stories, the badge that adds a story, the mark of the picked picture and the dark fades behind the text of a story. |

## Package layout

```text
photo-social/
  app.json               Window, physical UI scale, any orientation and the identifier.
  source/
    main.lua             Registers the theme, the actions of the stories and the pictures, and starts the shell.
    style.lua            The palette, the type scale, the spacing scale, the shapes and the theme.
    shell.lua            Tabs, navigation, notices, the rail and the bar of icons, the wordmark and the headers.
    page.lua             The base of every screen.
    widgets.lua          The templates of posts, comments, people, stories and tiles, and the dialog that sends a post.
    photos.lua           The twelve pictures, their looks and the filters.
    avatars.lua          The faces of the people.
    compose.lua          The post being made and its previews.
    chats.lua            The conversations and the answers.
    random.lua           The seeded generator of the fake data.
    tour.lua             The automatic run of the headless platform.
    data/social.lua      The people, their posts, comments and stories, and the person who uses the app.
    screens/             Home, story, post, profile, explore, the three steps of a new post, messages and chat.
  content/
    photos/              The twelve pictures.
    icons/               The line icons.
    images/              The logo and the marks.
    fonts/               Poppins and its license.
```
