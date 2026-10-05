-- Busy Interface: one dense screen of live cards, charts, long lists and a table, the way an operations dashboard looks, to show what a heavy interface costs.
local scene = require('haylen.scene')
local ui = require('haylen.ui')

ui.setTheme(ui.addTheme({
    name = 'operations',
    colors = {
        window = '#FF0F141C',
        panel = '#FF141B25',
        raised = '#FF1B2430',
        border = '#FF2A3646',
        borderStrong = '#FF3A4A60',
        text = '#FFE6ECF2',
        textMuted = '#FF8A9AAD',
        accent = '#FF3C9BE0',
        accentHover = '#FF57AEEB',
        accentStrong = '#FF2B7BBA',
        accentText = '#FF7FC0F0',
        accentBackground = '#333C9BE0',
        selection = '#403C9BE0',
        focus = '#FFE8A93A',
        success = '#FF35B37E',
        warning = '#FFE8A93A',
        danger = '#FFE5534B',
    },
    metrics = {
        controlHeight = 44,
        controlPaddingX = 14,
        controlPaddingY = 6,
        itemSpacing = 10,
        panelPadding = 14,
        panelRadius = 12,
        controlRadius = 8,
        listRowHeight = 38,
        progressHeight = 10,
        badgePaddingX = 10,
        badgePaddingY = 2,
        rowPadding = 10,
        iconSize = 24,
        toggleWidth = 58,
        toggleHeight = 30,
        cellRadius = 8,
    },
    fonts = {
        body = {size = 22},
        caption = {size = 18},
        button = {size = 22},
        heading = {size = 28},
        title = {size = 36},
        monospace = {size = 20},
    },
}, 'dark'))

scene.push(require('scenes.dashboard')())
