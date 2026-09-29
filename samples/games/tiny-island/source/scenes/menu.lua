-- The title screen over the island at dusk, with the best run so far.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local backdrop = require('systems.backdrop')
local preferences = require('systems.preferences')
local sound = require('systems.sound')
local widgets = require('ui.widgets')

local menu = {}
menu.__index = menu

-- Takes the backdrop of the screen it comes from, so the island keeps drifting without a reload.
function menu.new(scenery)
    return setmetatable({backdrop = scenery}, menu)
end

-- Loads what the menus draw and builds the island at dusk from it, unless the screen before handed its island over.
function menu:load(context)
    context:preload({'boot', 'menu'}):await()
    self.backdrop = self.backdrop or backdrop.new()
end

-- The title screen is the root screen, where the back button of a TV or an Android device leaves the app.
function menu:enter()
    window.setBackLeavesApp(true)
    self.backdrop:focus(nil)
    sound.music('menu')

    local buttons = {
        widgets.button('play', 'menu.play', function()
            scene.replace(require('scenes.class-select').new(self.backdrop))
        end, {sound = 'confirm'}),
        widgets.button('settings', 'menu.settings', function()
            scene.push(require('scenes.settings').new(function(dt)
                self.backdrop:update(dt)
            end))
        end, {variant = 'default'}),
    }
    if preferences.desktop() then
        buttons[#buttons + 1] = widgets.button('quit', 'menu.quit', haylen.quit, {variant = 'destructive'})
    end

    local best = preferences.best()
    self.document = ui.mount(ui.column{
        padding = 64,
        gap = 40,
        justify = 'center',
        ui.pageHeader{title = widgets.text('title'), banner = true, textAlign = 'center', align = 'center'},
        ui.column{width = 520, align = 'center', gap = 20, children = buttons},
        widgets.caption('best', widgets.text('menu.best', {count = best and best.days or 0})),
    })
    self.document:command('play', 'focus')
end

function menu:exit()
    self.document:unmount()
end

function menu:pause()
    self.document.visible = false
end

function menu:resume()
    window.setBackLeavesApp(true)
    self.document.visible = true
end

function menu:update(dt)
    self.backdrop:update(dt)
end

function menu:render()
    self.backdrop:draw()
end

return menu
