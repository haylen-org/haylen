# Music Streaming

Music Streaming is an app sample of a music service with a made-up library: 120 artists, 360 albums with covers drawn from shapes, more than 3,000 songs that the app makes with its own synthesizer, 2,400 liked songs, playlists, daily mixes, search, genres, a player bar on every screen, the now playing screen with seeking, shuffle, repeat and a sleep timer, and a queue to reorder. It looks like a product of its kind in deep violet, with a light theme, and it works with a mouse, touch, a keyboard, gamepads and TV remotes on phones, tablets, desktops and televisions. Every artist, album and song is fake data made from a seed, and every song is music made for the project by the code of the app, so the same library shows and sounds the same on every run.

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
| Headless player | `build/macos-debug/bin/haylen-headless samples/apps/music-streaming --frames 8000` |

On the headless platform the app runs a tour by itself: it plays a mix, opens the player, seeks, skips, shuffles, repeats, sets the sleep timer, likes a song, reorders the queue, scrolls the liked songs, opens an artist, adds a song to a playlist, searches, opens a genre and the library, makes a playlist, switches the theme, logs every step and quits.

## Screens

| Screen | What it shows |
| --- | --- |
| Home | A greeting with the settings, the card of the week, and shelves of playlists, daily mixes, new releases, popular artists and albums. |
| Search | The genres to browse while the field is empty, and the artists, albums and songs that match the typed text in one recycled list. |
| Library | The playlists, albums and artists in a recycled grid, and a dialog that makes a new playlist. |
| Album, playlist, mix and genre | The cover with the buttons that play and shuffle, over the songs in a recycled list that holds all 2,400 liked songs. |
| Artist | The picture, the listeners, the follow button, the popular songs and the albums in one recycled grid of mixed cells. |
| Now playing | The cover, the song with its heart, the position to drag, the controls with shuffle and repeat, the sleep timer and the volume. |
| Queue | The song that plays and the songs that come next, which a drag or the keyboard, a gamepad or a remote put in another order. |

Every song has a menu that a right click, a long press or the menu button opens: play next, add to the queue, add to a playlist, go to the artist and go to the album.

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

The screens, the shell with its tab bar and rail, the background and the navigation work as in the [food delivery sample](../food-delivery/README.md). Every screen with a list has the player bar of `Shell.playerBar`, which `Shell.showPlayer` updates from the player when the song or its state changes and moves its progress every frame, and the now playing screen rises from the bottom.

- The synthesizer of `source/synth.lua` makes a song from its seed and the sound of its genre: a chord progression on a pad, a bass, a melody with a hook that comes back, and drums, in 12 bars at the tempo of the genre. It renders 16-bit samples at 22,050 hertz inside a job of [`haylen.jobs`](../../../docs/lua-api/jobs.md), which pauses at checkpoints, so making a song never holds up a frame, and [`audio.newSound`](../../../docs/lua-api/audio.md#audionewsoundbytes-options) reads the samples. The plan of a song, which gives its length, costs nothing, so every list shows the real lengths before the songs exist.
- The autoload `source/player.lua` keeps the queue, plays a song with `audio.playMusic`, which crossfades from the one before and starts at a position, seeks with `audio.setCursor`, keeps the last six songs and makes the next one ahead of time, goes on to the next song when one ends, and shows what plays in the now playing controls of the system with `system.setNowPlaying`, whose `mediaCommand` events play, pause, skip and seek. Every change emits `playerChanged` on [`haylen.events`](../../../docs/lua-api/events.md).
- The covers of `source/covers.lua` are SVG documents made from one of six patterns and the colors of a genre, read with `graphics.newVectorImage` and named with `ui.setImage`, so 368 covers take a few hundred bytes each and rasterize only while they show.
- The menu of a song is a `contextMenu` around the row in the template of the cell, so its `select` arrives on the collection with `part` and `cell`, and the queue is a `collection` with `reorderable = true`, whose `reorder` event moves the song in the player.

## Art

The art follows the style guide of Tiny Island, the standard of the repository: chunky cute shapes, a bold navy outline `#1E1B33`, soft cel shading and one highlight, light from the top left and bright saturated colors.

| Part | Rule |
| --- | --- |
| Covers | Made by `source/covers.lua`: a diagonal gradient of the two colors of a genre with a sun over stripes, rings, bands, dots, waves or overlapping circles in a third color, outlined in navy, turned by a quarter of a turn, with a soft white sheen at the top. |
| Icons | `content/icons/`: line icons of 24 units with strokes of 2 in `currentColor`, and filled play, pause, next and previous, which the buttons, `ui.icon` and the items of menus tint. |
| Logo | `content/images/logo.svg`: white headphones with yellow ear cups on a violet tile with the navy outline. |
| Illustration | `content/images/hero.png`: the one painted image of the app, violet headphones on a vinyl record with a cassette and music notes, generated with the `codex` command line and its `$imagegen` skill in 1024 by 768 pixels with a transparent background and scaled to 768 by 576. It shows on the card of the week. |
| Sound | Every song is made by the synthesizer of the app, so the sample holds no recordings. |
| Palette | Night violet `#120E1F` and `#1E1832` surfaces, text `#F3EEFF` and `#A49BC0`, violet `#9B6BFF` as the accent and a yellow focus ring, and the light theme on `#F7F4FF`. |
| Font | Kanit, in its regular and semibold faces, under the SIL Open Font License in `content/fonts/kanit-license.txt`. |

## Package layout

```text
music-streaming/
  app.json               Window, physical UI scale, any orientation, the playback audio session, the player autoload and the identifier.
  source/
    main.lua             Registers the font, the night and light themes and the covers, and starts the shell.
    shell.lua            Tabs, navigation, notices, the rail, the tab bar, the player bar and centered columns.
    page.lua             The base of every screen.
    widgets.lua          The templates of songs, covers and artists, the menu of a song and the playlist dialog.
    player.lua           The queue and the playback, an autoload.
    synth.lua            The synthesizer of the songs.
    covers.lua           The covers of the albums and mixes.
    random.lua           The seeded generator of the fake data.
    tour.lua             The automatic run of the headless platform.
    data/catalog.lua     The genres, artists, albums, songs, playlists and mixes.
    screens/             Home, search, library, collection, artist, now playing and queue.
  content/
    icons/               The line icons.
    images/              The logo and the illustration.
    fonts/               Kanit and its license.
```
