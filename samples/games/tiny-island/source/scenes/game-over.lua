-- The end of a run over the frozen island, with how long it lasted and whether it beat the record.
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local widgets = require('ui.widgets')

local gameOver = {}
gameOver.__index = gameOver

function gameOver.new(run, days, kills, record)
    return setmetatable({run = run, days = days, kills = kills, record = record, transparent = true}, gameOver)
end

function gameOver:enter()
    self.document = ui.mount(ui.column{
        justify = 'center',
        padding = 64,
        gap = 28,
        ui.pageHeader{title = widgets.text('gameOver.title'), banner = true, textAlign = 'center', align = 'center'},
        widgets.caption('days', widgets.text('gameOver.days', {count = self.days})),
        widgets.caption('kills', widgets.text('gameOver.kills', {count = self.kills})),
        ui.label{text = widgets.text('gameOver.record'), font = 'heading', color = 'focus', outline = '#FF1B1E2B', outlineWidth = 3, align = 'center', visible = self.record},
        ui.row{
            justify = 'center',
            gap = 32,
            ui.column{width = 400, widgets.button('menu', 'gameOver.menu', function()
                self:leave(require('scenes.menu').new())
            end, {variant = 'default', sound = 'back'})},
            ui.column{width = 400, widgets.button('retry', 'gameOver.retry', function()
                self:leave(require('scenes.gameplay').new(self.run.class))
            end, {sound = 'confirm'})},
        },
    })
    self.document:command('retry', 'focus')
end

function gameOver:leave(next)
    if not self.leaving then
        self.leaving = true
        self.run:leave(next)
        scene.pop()
    end
end

function gameOver:exit()
    self.document:unmount()
end

return gameOver
