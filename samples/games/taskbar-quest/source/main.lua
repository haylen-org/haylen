-- Taskbar Quest: a tiny hero walks and fights in a transparent strip above the taskbar, while every click the game does not need reaches the desktop behind it.
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

input.loadActions({actions = {
    {name = 'strike', type = 'button', bindings = {'mouse:left'}},
    {name = 'quit', type = 'button', bindings = {'key:escape'}},
}})
ui.setTheme(ui.loadTheme('ui/theme.json'))

scene.push(require('scenes.game').new())
