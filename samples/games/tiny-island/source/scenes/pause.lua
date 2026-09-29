-- The pause menu over the dimmed run. It pauses the engine while it is open and runs only while the engine is paused, so the run below stands still.
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local sound = require('systems.sound')
local widgets = require('ui.widgets')

local pause = {}
pause.__index = pause

function pause.new(run)
    return setmetatable({run = run, transparent = true, processMode = 'whenPaused'}, pause)
end

function pause:enter()
    haylen.setPaused(true)
    self.document = ui.mount(ui.column{
        justify = 'center',
        padding = 64,
        ui.panel{
            width = 640,
            align = 'center',
            gap = 20,
            ui.label{text = widgets.text('pause.title'), font = 'title', textAlign = 'center', align = 'center'},
            widgets.button('resume', 'pause.resume', function()
                self:close()
            end),
            widgets.button('settings', 'pause.settings', function()
                scene.push(require('scenes.settings').new())
            end, {variant = 'default'}),
            widgets.button('quit', 'pause.quit', function()
                self.run:leave(require('scenes.menu').new())
                self:close()
            end, {variant = 'destructive', sound = 'back'}),
        },
    })
    self.document:command('resume', 'focus')
end

function pause:close()
    if not self.closing then
        self.closing = true
        scene.pop()
    end
end

function pause:exit()
    self.document:unmount()
    haylen.setPaused(false)
end

function pause:pause()
    self.document.visible = false
end

function pause:resume()
    self.document.visible = true
end

function pause:update(dt)
    if input.pressed('pause') then
        sound.play('back')
        self:close()
    end
end

return pause
