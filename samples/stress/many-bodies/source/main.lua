-- Many Bodies: a toy bin that thousands of physics bodies of four shapes pour into, with blasts and a shaking bin, drawn from one sprite batch.
local scene = require('haylen.scene')
local ui = require('haylen.ui')

ui.setTheme(ui.addTheme({name = 'playroom', colors = {
    panel = '#EB221A38',
    raised = '#FF332850',
    border = '#FF4A3C70',
    borderStrong = '#FF5F4E8C',
    accent = '#FF3D7FD9',
    accentHover = '#FF5A95E6',
    accentStrong = '#FF2A5CA8',
    accentText = '#FF8DB8F5',
    accentBackground = '#333D7FD9',
    selection = '#403D7FD9',
    focus = '#FFF2B630',
    text = '#FFF6F0FF',
    textMuted = '#FFB3A8D1',
}}, 'dark'))

scene.push(require('scenes.playroom')())
