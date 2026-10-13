-- The tests of the category in menu order.
return {
    prefix = 'VID',
    title = 'Video',
    description = 'Video files decoded by the decoder of each platform into a texture that sprites and the UI draw, with their sound through the mixer.',
    tests = {
        {code = 'VID-001', title = 'Video playback', description = 'A clip that plays, pauses, seeks, loops and changes its rate and volume, drawn as sprites and as an image of the UI, with its sound on the music bus.', module = 'playback', platforms = {'macos', 'windows', 'ios', 'tvos', 'android', 'web'}, unsupported = {linux = 'Linux has no decoder of video files that every distribution ships.', headless = 'The headless platform decodes no video files.'}},
    },
}
