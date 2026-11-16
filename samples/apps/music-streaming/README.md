# 🎵 Music Streaming

Music Streaming is an app sample of a music service with a made-up library: 120 artists, 360 albums with covers drawn from shapes, more than 3,000 songs that the app makes with its own synthesizer, 2,400 favorite songs, playlists, a mix of every genre, search, genres, a player that follows every screen, the now playing screen with a visualizer, seeking, shuffle, repeat and a sleep timer, and a queue to reorder. It looks like a product of its kind in a dark and immersive look lit by the colors of the music, and it works with a mouse, touch, a keyboard, gamepads and TV remotes on phones, tablets, desktops and televisions. Every artist, album and song is fake data made from a seed, and every song is music made for the project by the code of the app, so the same library shows and sounds the same on every run.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/apps/music-streaming` |
| macOS app | `haylen run samples/apps/music-streaming --platform macos` |
| iPhone and iPad simulator | `haylen run samples/apps/music-streaming --platform ios-simulator` |
| Apple TV simulator | `haylen run samples/apps/music-streaming --platform tvos-simulator` |
| Android device or emulator | `haylen run samples/apps/music-streaming --platform android --device <serial>` |
| Browser | `haylen run samples/apps/music-streaming --platform web` |
| Headless player | `build/macos-debug/bin/haylen-headless samples/apps/music-streaming` |

On the headless platform the app runs a tour by itself: it plays a mix, opens the player, seeks, picks a shorter song near the end of the one that plays and checks that the time of the old song never counts for the new one, skips, shuffles, repeats, sets the sleep timer, likes a song, shares it, pulls the queue up from half the screen, reorders it and removes a song, scrolls the favorites, opens an artist, adds a song to a playlist and takes one out, searches, opens a genre and the library, makes a playlist and deletes it after the alert, opens links of the scheme and of the web, logs every step and quits.

## Screens

| Screen | What it shows |
| --- | --- |
| Home | A greeting, the mix of the day on a card in the colors of its genre, and shelves of the mixes, the playlists, new releases, popular artists and albums played lately. |
| Search | The genres as tiles in their colors while the field is empty, and the artists, albums and songs that match the typed text in one recycled list. |
| Library | The playlists, albums and artists in a recycled grid, picked with pills, a dialog that makes a new playlist and, where quitting closes the app, a button that quits it. |
| Album, playlist, mix and genre | The big cover, the names and the buttons that play and shuffle under the glow of the colors of the genre, over the songs in a recycled list that holds all 2,400 favorites, with the share button in the bar and, on a playlist of the person, the button that deletes it after an alert. A link to a song shows its album around the song. |
| Artist | The round picture, the listeners, the follow button, the popular songs and the albums in one recycled grid of mixed cells, with the share button in the bar. |
| Now playing | The big cover over the glow of its genre, the visualizer, the song with its heart, the position to drag, the controls with shuffle and repeat, the sleep timer, the volume and the share button of the song. A wide window shows the cover beside the controls and the songs that come next. |
| Queue | A sheet that rests at half the screen and opens to nearly all of it, with the song that plays and the songs that come next, which a drag or the keyboard, a gamepad or a remote put in another order, and which leave from their menu or all at once with a snackbar that brings them back. |

Every song has a menu that a right click, a long press or the menu button opens: play next, add to the queue, add to a playlist, go to the artist, go to the album and share, and on a playlist of the person, remove from the playlist, with a snackbar that puts the song back.

## Links

The links of the scheme `haylenmusic` and of the web host `music.example.com` open the screens of the app from a cold start and while it runs, through the routes of `source/routes.lua`. The local plugin of `plugins/music-links/` registers the scheme on Apple platforms, Android, Windows and Linux, and its README lists the commands that open a link on each of them. The share buttons and the menu of a song put either link on the clipboard.

| Link | What it opens |
| --- | --- |
| `haylenmusic://home` | The home tab. |
| `haylenmusic://search` | The search tab. |
| `haylenmusic://search/golden` | The results of a search over the search tab, with the way back. |
| `haylenmusic://library` | The library tab. |
| `haylenmusic://artist/12` | An artist over the root of the tab that shows. |
| `haylenmusic://album/30` | An album over the root of the tab that shows. |
| `haylenmusic://album/30?track=264` | An album around one of its songs, the link that sharing a song gives. |
| `haylenmusic://playlist/drive` | A playlist or a mix, such as `playlist/mix-jazz` or the favorites, `playlist/liked`. |
| `haylenmusic://genre/jazz` | The songs of a genre over the search tab. |
| `haylenmusic://player` | The player over everything. |
| `haylenmusic://queue` | The queue in its sheet. |
| `https://music.example.com/artist/12` | The same screens from the web host, for every path above. |

A link to something the library lacks, such as `haylenmusic://artist/9999`, shows a page that says so, and a link that no route matches leaves the app as it is.

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Move between controls, cells and tabs | The arrows and Tab, or the pointer | The directional pad and the left stick | A tap | The directions |
| Press, play a song | Enter, Space or a click | The south button | A tap | Select or a click |
| Open the menu of a song | The menu key or a right click | The north button | A long press | The menu of the focus |
| Move a song of the queue | A drag, or Enter twice, the arrows and Enter | The south button twice, the directions and the south button | A long press and a drag | Select twice, the directions and select |
| Go back | Escape or the back button | The east button | The back button | Back or Menu |
| Play, pause and skip | The media keys | The buttons of the player | The buttons of the player | Play and pause |

## How it works

The app follows the [navigation](../../../docs/ui/navigation.md) of the engine, the way an app of a phone or a tablet moves between its screens. The scene of `source/shell.lua` mounts the frame once, the sidebar, the player bar, the floating card of the song and the bar of tabs, and its tabs, `ui.navigationTabs` in the middle of the frame, keep a stack each: going to an artist or an album pushes a screen in the stack of the tab that shows with a slide that moves only the middle, a tab keeps its screens, its scroll and its fields while another tab shows, and selecting the tab that shows goes back to its root. The tab buttons change only their look when the tab changes, so the bars are never built again. The now playing screen and the queue open over everything as presentations, the player over the whole screen with its own bar and the queue as a sheet, so they start a flow of their own and the system back closes them before it goes back in the tab. The screens of `source/screens/` are classes of `ui.Screen` built on the base of `source/page.lua`, which builds their tree over the glow of their genre and runs their start the first time they show. An artist or an album that already shows in the stack comes back instead of opening a second copy, and the back button of a screen, the back button of Android, Escape and the east button go back the same way. A phone shows the bar of tabs at the bottom with a floating card of the song that plays above it, and a wide window shows a sidebar with the tabs and the playlists on the left and the player bar of a desktop across the bottom, whose position the person drags.

- The synthesizer of `source/synth.lua` makes a song from its seed and the sound of its genre: a chord progression on a pad, a bass, a melody with a hook that comes back, and drums, in 12 bars at the tempo of the genre. It renders 16-bit samples at 22,050 hertz inside a job of [`haylen.jobs`](../../../docs/lua-api/jobs.md), which pauses at checkpoints, so making a song never holds up a frame, and [`audio.newSound`](../../../docs/lua-api/audio.md#audionewsoundbytes-options) reads the samples. The plan of a song, which gives its length, costs nothing, so every list shows the real lengths before the songs exist.
- The visualizer reads the same plan: `Synth.levels` says how loud the kick, the snare, the hats, the bass, the melody and the pad sound at the time the song reached, from the envelopes that render them, so 36 bars follow the music without listening to it. The now playing screen leaves a spacer where the bars go and draws them there with [`haylen.graphics2d`](../../../docs/lua-api/graphics2d.md), from violet at the low end to pink at the high end, rising fast and falling back slowly.
- The autoload `source/player.lua` keeps the queue, plays a song with `audio.playMusic`, which crossfades from the one before and starts at a position, seeks with `audio.setCursor`, keeps the last six songs and makes the next one ahead of time, goes on to the next song when one ends, and shows what plays in the now playing controls of the system with `system.setNowPlaying`, whose `mediaCommand` events play, pause, skip and seek. Every change emits `playerChanged` on [`haylen.events`](../../../docs/lua-api/events.md).
- The covers of `source/covers.lua` are SVG documents made from one of six patterns over a diagonal gradient of the colors of a genre, read with `graphics.newVectorImage` and named with `ui.setImage`, so 368 covers take a few hundred bytes each and rasterize only while they show. The glow behind a screen is a tall texture of `image:fillGradient` that fades from a dark shade of the genre into the night.
- The menu of a song is a `contextMenu` around the row in the template of the cell, so its `select` arrives on the collection with `part` and `cell`, and the queue is a `collection` with `reorderable = true`, whose `reorder` event moves the song in the player.
- The router of `source/routes.lua`, `ui.router` over the tabs with `restore = true`, turns links into screens, and every screen that the person opens goes through it too with `router:push`, so the router knows the location of each screen, builds the stacks of the tabs and the presentations again after the system ended the app, and gives the location that sharing puts in a link. The player and the queue are routes that present their screens, the player over the whole screen and the queue as a page sheet with the detents `medium` and `large`, as the [navigation guide](../../../docs/ui/navigation.md#sheets-and-their-detents) describes.
- Sharing is an action sheet of `ui.presentAlert` that points at the share button on a wide window and rises from the bottom of a phone, deleting a playlist asks first in an alert with a destructive action, and a song that leaves a playlist or the queue shows a snackbar of `ui.showMessage` whose `Undo` puts it back where it was, while the other confirmations are toasts at the top, clear of the player.

## Style guide

| Part | Rule |
| --- | --- |
| Palette | Night `#0B0A12` with surfaces `#17151F` and `#221F2D`, text `#F5F3FF` and `#A19DB5`, violet `#8B5CFF` for the buttons and the low bars of the visualizer, pink `#FF4D9D` for its high bars, a yellow `#FFD24A` focus ring, and the colors of the genres in the covers, the tiles of the search and the glow behind each screen. |
| Type scale | Kanit, in its regular and semibold faces. Captions 18 units, body 22, labels 21 in semibold, headings 31, titles 43 and the names of a collection or an artist 59 on wide windows, sizes of the em square, which the theme takes as they are, about 13.5, 16.5, 16, 23, 32 and 44 points. |
| Spacing scale | Steps of 4, 8, 12, 16, 24, 32 and 48 UI units. Shelves keep 8 between their covers, pages keep 16 at their sides on a phone and 32 on a wide window, and sections keep 24 above their titles. |
| Shapes | Round controls and pills, round play buttons of 56 to 92, artists in circles, covers with small corners of 8 and cards of 14. |
| Sizes | Covers of 200 in the shelves and up to 560 on the now playing screen, controls 60 tall, the bar of tabs 88 tall, the sidebar 300 wide and the player bar of a desktop 112 tall. |
| Covers | Made by `source/covers.lua`: a diagonal gradient of the two colors of a genre with a sun over stripes, rings, bands, dots, waves or overlapping circles in a third color, outlined in navy `#1E1B33`, turned by a quarter of a turn, with a soft white sheen at the top. |
| Icons | `content/icons/`: bold rounded icons of 24 units with strokes of 2.3 in `currentColor`, filled for the controls of the player and for the tab that shows, which the buttons, `ui.icon` and the items of menus tint. |
| Logo | `content/images/logo.svg`: white headphones with yellow ear cups on a violet tile with the navy outline. |
| Illustration | `content/images/hero.png`: the one painted image of the app, violet headphones on a vinyl record with a cassette and music notes, generated with the `codex` command line and its `$imagegen` skill in 1024 by 768 pixels with a transparent background and scaled to 768 by 576. It shows on the mix of the day of wide windows. |
| Sound | Every song is made by the synthesizer of the app, so the sample holds no recordings. |

## Package layout

```text
music-streaming/
  app.json               Window, physical UI scale, any orientation, the playback audio session, the player autoload, the plugin of the links and the identifier.
  source/
    main.lua             Registers the theme and the covers, and starts the shell.
    style.lua            The palette, the type scale, the spacing scale, the shapes and the theme.
    shell.lua            Tabs, navigation, messages, alerts, sharing, the sidebar, the bar of tabs, the floating card of the song and the player bar.
    routes.lua           The routes of the links and the locations of the screens.
    page.lua             The base of every screen, a class of "ui.Screen", with the glow of its genre.
    widgets.lua          The templates of songs, covers and artists, the shelves, the menu of a song and the playlist sheet.
    player.lua           The queue and the playback, an autoload.
    synth.lua            The synthesizer of the songs and the levels of their voices.
    covers.lua           The covers of the albums and mixes, and the glow of the genres.
    random.lua           The seeded generator of the fake data.
    tour.lua             The automatic run of the headless platform.
    data/catalog.lua     The genres, artists, albums, songs, playlists and mixes.
    screens/             Home, search, library, collection, artist, now playing and queue.
  content/
    icons/               The icons.
    images/              The logo and the illustration.
    fonts/               Kanit and its license.
  plugins/
    music-links/         The local plugin that registers the scheme of the links.
```
