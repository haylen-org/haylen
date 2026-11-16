# 📷 Photo Social

Photo Social is an app sample of a photo sharing network: a feed of posts with pictures, likes, comments and captions, stories that play full screen, profiles with a grid of pictures and a follow button, a post with its comments, a new post with a picture, a crop, a filter and a caption, and direct messages with a real chat. It looks like a product of its kind in a bright white look of rounded prints, with a ring that runs from blue to teal around the stories and a blue accent, and it works with a mouse, touch, a keyboard, gamepads and TV remotes on phones, tablets, desktops and televisions. Every person, post, comment and message is fake data made from a seed, so the same feed shows on every run.

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

On the headless platform the app runs a tour by itself: it likes a post, watches and answers a story, shares a post with a friend, whose answer drops in as a banner, comments on a post and deletes the comment after the alert, follows and unfollows a person, searches people, chats, makes a post with a filter and a crop and deletes it after the alert, edits the profile, saves posts, opens links of the scheme and of the web and sends a post to a friend, logs every step and quits.

## Screens

| Screen | What it shows |
| --- | --- |
| Home | The stories in a row, with the person's own first, over the feed of the people the person follows. Every post shows its author with the place and the time, the caption over the picture, and the buttons with the counts of likes and comments under it. A phone shows the posts under the wordmark, and a wide window shows them as cards in a column, with the person and suggestions of people to follow beside it when there is room. |
| Story | The pictures of a person, five seconds each, under bars that fill as they play, then the stories of the next person. A tap on the left third goes back and anywhere else goes on, a press holds the story, and the field at the bottom replies in the direct messages. A wide window shows the pictures in a tall frame between two arrows. |
| Post | The post and its comments over the field that adds one, or on a wide window the picture on a dark stage beside the author, the comments, the buttons and the field. A post of the person has the button that deletes it, and a comment of the person has a menu that deletes it, both after an alert. |
| Profile | The face inside the ring of the app, the name and the bio in the middle, the counts of posts, followers and following on a soft card, the buttons that follow and message, and the grid of pictures. Unfollowing shows a snackbar that follows again, and the message button opens the chat in the messages tab. The person's own profile edits its name and bio in a sheet that asks before it drops changes, starts a post and shows the saved posts in a second section. |
| Explore | A grid of the pictures of everyone, and the people whose name or handle matches the typed text. |
| New post | Three steps: a picture of the library, its crop and its filter, each shown on the middle of the picture, and the caption and the place before it goes to the top of the feed. |
| Messages | The conversations, the newest on top, with a dot on those with messages not read yet, narrowed by a typed name. |
| Chat | The messages in a recycled list that sticks to its end, with the posts that travel in them as pictures, and the field that sends a message. The other person answers a moment later. |

Every post has a menu with the way to the profile, sending it to a friend in a sheet and saving it. Its share button offers sending it to a friend and copying its link in an action sheet, which points at the button on a wide window.

## Links

The links of the scheme `haylenphotos` and of the web host `photos.example.com` open the screens of the app while it runs, through the routes of `source/routes.lua`, and the action sheet of a post copies them. A platform opens a link of the scheme once a plugin registers it, as the local plugin of [Music Streaming](../music-streaming/plugins/music-links/README.md) does for its own.

| Link | What it opens |
| --- | --- |
| `haylenphotos://home` | The home tab. |
| `haylenphotos://explore` | The explore tab. |
| `haylenphotos://messages` | The messages tab. |
| `haylenphotos://profile` | The person's own profile. |
| `haylenphotos://profile/ava` | The profile of a person over the root of the tab that shows. |
| `haylenphotos://post/46` | A post with its comments over the root of the tab that shows. |
| `haylenphotos://chat/kai` | The chat with a person in the messages tab, beside the conversations on a wide window and over them on a phone. |
| `https://photos.example.com/post/46` | The same screens from the web host, for every path above. |

A link to someone or something the app lacks, such as `haylenphotos://post/9999`, shows a page that says so, and a link that no route matches leaves the app as it is.

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Move between controls, cells and tabs | The arrows and Tab, or the pointer | The directional pad and the left stick | A tap | The directions |
| Press | Enter, Space or a click | The south button | A tap | Select or a click |
| Step through the stories | The left and right arrows, or a click on the picture | The directional pad and the left stick | A tap on the picture | The directions |
| Hold a story | A press on the picture | | A press on the picture | |
| Go back | Escape or the back button | The east button | The back button | Back or Menu |

## How it works

The app follows the [navigation](../../../docs/ui/navigation.md) of the engine, the way an app of a phone or a tablet moves between its screens. The scene of `source/shell.lua` mounts the frame once, the rail of a wide window and the bar of icons of a phone, and its tabs, `ui.navigationTabs` in the middle of the frame, keep a stack each, so a post or a profile slides in only the middle while the bars stay, a tab keeps its screens and its scroll while another tab shows, and selecting the tab that shows goes back to its root. The tab buttons change only their look when the tab changes, and the count of unread messages lives on them. The messages tab is a split of the conversations and the open chat, side by side on a wide window with a note until a chat opens, and one stack on a phone, where the chat slides over the conversations, and the chat moves between the two as the window crosses the wide width. A new post and the stories open over everything as presentations that cover the whole screen: the new post has its own bar with the way out, the title of the step and its action, which stays while the picture, the edit and the caption slide under it in a stack of their own. The screens of `source/screens/` are classes of `ui.Screen` built on the base of `source/page.lua`. A profile or a conversation that already shows in the stack comes back instead of opening a second copy, and the back buttons, the back button of Android, Escape and the east button go back the same way, through the stack, then to the home tab, and they close a presentation before anything else.

- The pictures of `source/photos.lua` are twelve painted files that every post, story and profile reuses. Each one shows whole or as a close crop around its subject with a filter, made on the task pool with `image:crop` and `image:applyAsync` of [`haylen.graphics`](../../../docs/lua-api/graphics.md) and named for the UI with `ui.setImage`, so twelve files give twenty-four looks. Until a look is ready its name shows the mean color of its picture, so nothing waits on the frame thread, and the textures keep mipmaps, so the small tiles of a grid stay smooth.
- The faces of `source/avatars.lua` are SVG documents drawn from a few shapes, a round face with dots for eyes, rosy cheeks and a smile under one of six haircuts, read with `graphics.newVectorImage`, so every person has a face of a few hundred bytes that stays sharp at every size.
- The tab of a new post is a round button in the gradient of the app, and like the face of the profile it keeps its own colors while the other tabs take the color of the text.
- The feed, the comments, the grids and the conversations are `collection` nodes. The posts of a phone and of a wide window are two templates of the same items, the heart over a liked picture is a part whose [transform](../../../docs/lua-api/ui.md#guitransformid) a tween fades and grows, and the head of a profile is the first cell of its grid, across all three columns.
- The story is a [play area](../../../docs/lua-api/ui.md#uiplayareaproperties) over the picture, so the arrows, the directional pad and a remote reach the actions `storyPrevious` and `storyNext` of the action map while the reply field and the buttons stay a Tab away. A press on the picture reports `press` and `release`, which hold the story and step through it.
- The new post decodes the chosen picture on the task pool, makes the small previews of every filter from its middle and the preview of the post with the crop and the filter, all in tasks that the screen owns.
- The data of `source/data/social.lua` and the conversations of `source/chats.lua` emit `socialChanged` and `chatChanged` on [`haylen.events`](../../../docs/lua-api/events.md), and the screens listen with themselves as owners. The answer of a friend arrives through a timer owned by the conversations, so it reaches the messages after the chat closed, and an answer in a chat that does not show emits `chatAnswered`, which drops a banner of `ui.showMessage` with the face of the friend under the status bar, whose tap opens the chat.
- The router of `source/routes.lua`, `ui.router` over the tabs with `restore = true`, turns links into screens, and the profiles, posts and chats that the person opens go through it too, so the router knows the location of each screen, builds the stacks and the open chat again after the system ended the app, and gives the location that sharing puts in a link. The route of a chat is a detail of the split of the messages tab, and the banner of an answer and the message button of a profile open it with `router:open`, which shows the messages tab with the chat.
- Sharing is an action sheet of `ui.presentAlert`, and deleting a post or a comment asks first in an alert with a destructive action. Taking a post out of the collection and unfollowing a person show a snackbar whose `Undo` takes it back, and the other confirmations are toasts at the bottom, which keep above the tab bar through `ui.setMessageArea`.
- The field of a chat stays above the on-screen keyboard, and a tap on the send button while the keyboard is open sends at once, because the GUIs hold their place while a press is down.

## Style guide

| Part | Rule |
| --- | --- |
| Palette | Paper white `#FFFFFF` with a soft gray `#F4F4F7` for incoming bubbles, ink `#141418` and gray `#71717E` for the text, hairlines `#EBEBF0`, blue `#2F6BFF` for the buttons, the focus and the bubbles of the person, red `#FF3358` for a liked heart, and the gradient of the app from blue `#2F6BFF` to teal `#14C8B4` for the ring of the unseen stories and of a profile, the button of a new post and the badge that adds a story. |
| Type scale | Poppins, in its regular and semibold faces. Captions 18 units, body 21, labels 21 in semibold, headings 26, titles 35 and the wordmark 43, sizes of the em square, which the theme takes as they are, about 13.5, 16, 16, 20, 26 and 32 points. |
| Spacing scale | Steps of 4, 8, 12, 16, 24, 32 and 48 UI units. Rows and pictures of a post keep 16 at their sides on a phone and 24 in a card, the feed of a wide window keeps 24 between its cards, and the tiles of a grid keep 8 between them. |
| Shapes | Pictures are prints with corners of 16 inside the margins of the feed, and the tiles of a grid round to 10. Buttons are rounded squares of 14, the fields and the bubbles round to 26, faces are circles, and the bar of a phone shows icons alone, filled while they show, around the round button of a new post. |
| Sizes | Faces of 56 in posts and 44 in comments, stories of 112, controls 60 tall, the bar of icons 84 tall, the rail of a wide window 300 wide and the feed of a wide window at most 640 wide. |
| Pictures | `content/photos/`: twelve photo-like illustrations of 768 by 768 pixels painted for the app in the style of the repository, landscapes, food, pets and the city, with a clean dark navy outline `#1E1B33`, soft cel shading with two tones and one highlight, light from the top left and bright colors, generated with the `codex` command line and its `$imagegen` skill and saved as JPG files. |
| Faces | Drawn by `source/avatars.lua` with the same navy outline, in six skin tones, seven hair colors and eight shirts on pastel backgrounds. |
| Icons | `content/icons/`: line icons of 24 units with strokes of 2 in `currentColor`, with bolder or filled versions for the tab that shows, which the buttons and `ui.icon` tint. |
| Logo and marks | `content/images/`: the logo, two prints fanned on a blue tile, the front one a little landscape in blue and teal, with a teal badge that holds a white heart, in the navy outline and the soft shading of the repository, the rings of the stories, the button of a new post, the badge that adds a story, the mark of the picked picture and the dark fades behind the text of a story. |

## Package layout

```text
photo-social/
  app.json               Window, physical UI scale, any orientation and the identifier.
  source/
    main.lua             Registers the theme, the actions of the stories and the pictures, and starts the shell.
    style.lua            The palette, the type scale, the spacing scale, the shapes and the theme.
    shell.lua            Tabs, navigation, messages, alerts, the clipboard, the rail and the bar of icons, the wordmark and the headers.
    routes.lua           The routes of the links and the locations of the screens.
    page.lua             The base of every screen, a class of "ui.Screen".
    widgets.lua          The templates of posts, comments, people, stories and tiles, sharing, following and the sheet that sends a post.
    photos.lua           The twelve pictures, their looks and the filters.
    avatars.lua          The faces of the people.
    compose.lua          The post being made and its previews.
    chats.lua            The conversations and the answers.
    random.lua           The seeded generator of the fake data.
    tour.lua             The automatic run of the headless platform.
    data/social.lua      The people, their posts, comments and stories, and the person who uses the app.
    screens/             Home, story, post, profile, explore, the new post with its three steps, and messages with the inbox and the chat.
  content/
    photos/              The twelve pictures.
    icons/               The line icons.
    images/              The logo and the marks.
    fonts/               Poppins and its license.
```
