-- Many Sprites: a pond where a school of tens to hundreds of thousands of animated fish swims, drawn through the fast paths of sprite drawing.
local scene = require('haylen.scene')
local ui = require('haylen.ui')

ui.setTheme(ui.addTheme({name = 'pond', colors = {
    panel = '#E6102B33',
    raised = '#FF183E49',
    border = '#FF285866',
    borderStrong = '#FF3A7482',
    accent = '#FFE8633A',
    accentHover = '#FFF27A50',
    accentStrong = '#FFC8471F',
    accentText = '#FFFF9B62',
    accentBackground = '#33E8633A',
    selection = '#40E8633A',
    focus = '#FFF2C14E',
    text = '#FFEAF4F2',
    textMuted = '#FF9DBDBF',
}}, 'dark'))

scene.push(require('scenes.pond')())
