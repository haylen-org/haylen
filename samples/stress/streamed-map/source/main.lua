-- Streamed Map: a world of millions of tiles made from a seed and streamed in chunks around a camera that flies over it, built in the background and baked into static batches.
local scene = require('haylen.scene')
local ui = require('haylen.ui')

ui.setTheme(ui.addTheme({name = 'map', colors = {
    panel = '#E8172230',
    raised = '#FF223044',
    border = '#FF33465E',
    borderStrong = '#FF46607C',
    accent = '#FF2C8ACB',
    accentHover = '#FF46A0DC',
    accentStrong = '#FF1F6CA3',
    accentText = '#FF7FC4F2',
    accentBackground = '#332C8ACB',
    selection = '#402C8ACB',
    focus = '#FFF2C14E',
    text = '#FFF1F5F7',
    textMuted = '#FFA7B7C6',
}}, 'dark'))

scene.push(require('scenes.flight')())
