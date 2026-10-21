# Music Streaming

Music Streaming is an app sample of a music service with a made-up library: 120 artists, 360 albums with covers drawn from shapes, more than 3,000 songs that the app makes with its own synthesizer, 2,400 liked songs, playlists, daily mixes, search, genres, a player that follows every screen, the now playing screen with a visualizer, seeking, shuffle, repeat and a sleep timer, and a queue to reorder. It looks like a product of its kind in a dark and immersive look lit by the colors of the music, and it works with a mouse, touch, a keyboard, gamepads and TV remotes on phones, tablets, desktops and televisions. Every artist, album and song is fake data made from a seed, and every song is music made for the project by the code of the app, so the same library shows and sounds the same on every run.

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

On the headless platform the app runs a tour by itself: it plays a mix, opens the player, seeks, picks a shorter song near the end of the one that plays and checks that the time of the old song never counts for the new one, skips, shuffles, repeats, sets the sleep timer, likes a song, reorders the queue, scrolls the liked songs, opens an artist, adds a song to a playlist, searches, opens a genre and the library, makes a playlist, logs every step and quits.

## Screens

| Screen | What it shows |
| --- | --- |
| Home | A greeting, the playlists as quick picks with their covers, the mix of the day on a card in the colors of its genre, and shelves of mixes, new releases, popular artists and albums. |
| Search | The genres as tiles in their colors while the field is empty, and the artists, albums and songs that match the typed text in one recycled list. |
| Library | The playlists, albums and artists in a recycled grid, picked with pills, a dialog that makes a new playlist and, where quitting closes the app, a button that quits it. |
| Album, playlist, mix and genre | The big cover, the names and the buttons that play and shuffle under the glow of the colors of the genre, over the songs in a recycled list that holds all 2,400 liked songs. |
| Artist | The round picture, the listeners, the follow button, the popular songs and the albums in one recycled grid of mixed cells. |
| Now playing | The big cover over the glow of its genre, the visualizer, the song with its heart, the position to drag, the controls with shuffle and repeat, the sleep timer and the volume. A wide window shows the cover beside the controls and the songs that come next. |
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

The screens are scenes of `source/screens/` built on the base of `source/page.lua`, which mounts their GUI with the scene as owner, paints the night of the app with the glow of the genre a screen names and keeps the player of the frame current. The shell of `source/shell.lua` switches the tabs as the only screen without a transition, pushes the other screens with a slide and raises the now playing screen from the bottom. A phone shows the bar of tabs at the bottom with a floating card of the song that plays above it, and a wide window shows a sidebar with the tabs and the playlists on the left and the player bar of a desktop across the bottom, whose position the person drags.

- The synthesizer of `source/synth.lua` makes a song from its seed and the sound of its genre: a chord progression on a pad, a bass, a melody with a hook that comes back, and drums, in 12 bars at the tempo of the genre. It renders 16-bit samples at 22,050 hertz inside a job of [`haylen.jobs`](../../../docs/lua-api/jobs.md), which pauses at checkpoints, so making a song never holds up a frame, and [`audio.newSound`](../../../docs/lua-api/audio.md#audionewsoundbytes-options) reads the samples. The plan of a song, which gives its length, costs nothing, so every list shows the real lengths before the songs exist.
- The visualizer reads the same plan: `Synth.levels` says how loud the kick, the snare, the hats, the bass, the melody and the pad sound at the time the song reached, from the envelopes that render them, so 36 bars follow the music without listening to it. The now playing screen leaves a spacer where the bars go and draws them there with [`haylen.graphics2d`](../../../docs/lua-api/graphics2d.md), from violet at the low end to pink at the high end, rising fast and falling back slowly.
- The autoload `source/player.lua` keeps the queue, plays a song with `audio.playMusic`, which crossfades from the one before and starts at a position, seeks with `audio.setCursor`, keeps the last six songs and makes the next one ahead of time, goes on to the next song when one ends, and shows what plays in the now playing controls of the system with `system.setNowPlaying`, whose `mediaCommand` events play, pause, skip and seek. Every change emits `playerChanged` on [`haylen.events`](../../../docs/lua-api/events.md).
- The covers of `source/covers.lua` are SVG documents made from one of six patterns over a diagonal gradient of the colors of a genre, read with `graphics.newVectorImage` and named with `ui.setImage`, so 368 covers take a few hundred bytes each and rasterize only while they show. The glow behind a screen is a tall texture of `image:fillGradient` that fades from a dark shade of the genre into the night.
- The menu of a song is a `contextMenu` around the row in the template of the cell, so its `select` arrives on the collection with `part` and `cell`, and the queue is a `collection` with `reorderable = true`, whose `reorder` event moves the song in the player.

## Style guide

| Part | Rule |
| --- | --- |
| Palette | Night `#0B0A12` with surfaces `#17151F` and `#221F2D`, text `#F5F3FF` and `#A19DB5`, violet `#8B5CFF` for the buttons and the low bars of the visualizer, pink `#FF4D9D` for its high bars, a yellow `#FFD24A` focus ring, and the colors of the genres in the covers, the tiles of the search and the glow behind each screen. |
| Type scale | Kanit, in its regular and semibold faces. Captions 13.5 points, body 16.5, labels 16 in semibold, headings 23, titles 32 and the names of a collection or an artist 44 on wide windows, in points of the em square, which `source/style.lua` turns into the heights the theme takes. |
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
  app.json               Window, physical UI scale, any orientation, the playback audio session, the player autoload and the identifier.
  source/
    main.lua             Registers the theme and the covers, and starts the shell.
    style.lua            The palette, the type scale, the spacing scale, the shapes and the theme.
    shell.lua            Tabs, navigation, notices, the sidebar, the bar of tabs, the floating card of the song and the player bar.
    page.lua             The base of every screen, with the glow of its genre.
    widgets.lua          The templates of songs, covers and artists, the shelves, the menu of a song and the playlist dialog.
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
```
