-- Autoloads: the player data that app.json loads before main.lua, shared by this test, a shop scene and the coin counter it draws on every screen, and a jukebox added at run time with haylen.autoload.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Journal = require('journal')
local sample = require('sample')

local Autoloads = haylen.class('Autoloads', sample.Test)

local kCode = [[
"autoload": ["state.player-data"]  -- in app.json, loaded before source/main.lua
local playerData = haylen.autoloads.playerData  -- the same table as require('state.player-data')
haylen.autoload('state.jukebox')  -- one more autoload at run time, named jukebox]]

local kItems = {{id = 'hat', text = 'Buy a hat for 25', price = 25}, {id = 'scarf', text = 'Buy a scarf for 15', price = 15}}

-- The shop spends the coins of the same player data the test shows.
local Shop = haylen.class('Shop', sample.Overlay)

function Shop:enter(params)
    local data, journal = haylen.autoloads.playerData, params.journal
    local children = {ui.label{id = 'coins', text = data.coins .. ' coins', color = 'warningText'}}
    for _, item in ipairs(kItems) do
        children[#children + 1] = ui.button{id = item.id, text = item.text, onClick = function()
            if data:spend(item.price) then
                table.insert(data.hats, item.id)
                journal:add('the shop sold a ' .. item.id .. ', ' .. data.coins .. ' coins left', sample.green)
            else
                journal:add('not enough coins for a ' .. item.id, sample.red)
            end
            self.document:set('coins', {text = data.coins .. ' coins'})
        end}
    end
    self:card('The shop', children)
end

function Autoloads:enter()
    self.journal = Journal()
    self:listen('autoload_started', function(info) self.journal:add('autoload_started ' .. info.name, sample.accent) end)
    self:frame({
        hint = 'Earn coins here and spend them in the shop. The counter in the corner is drawn by the autoload on every screen.',
        code = kCode,
        controls = {
            ui.button{id = 'earn', text = 'Earn 10 coins', variant = 'primary', onClick = function()
                haylen.autoloads.playerData:earn(10)
                self.journal:add('earned 10 coins')
            end},
            ui.button{id = 'shop', text = 'Open the shop', onClick = function() sample.overlay(Shop(), {journal = self.journal}) end},
            ui.button{id = 'jukebox', text = 'Add the jukebox autoload', enabled = haylen.autoloads.jukebox == nil, onClick = function() self:addJukebox() end},
        },
        focus = 'earn',
    })
end

function Autoloads:addJukebox()
    haylen.autoload('state.jukebox')
    self:set('jukebox', {enabled = false})
end

function Autoloads:update(dt)
    Autoloads.super.update(self, dt)
    local names = {}
    for name in pairs(haylen.autoloads) do
        names[#names + 1] = name
    end
    table.sort(names)
    self:status('haylen.autoloads: ' .. table.concat(names, ', '))
end

function Autoloads:draw(area)
    local data, jukebox = haylen.autoloads.playerData, haylen.autoloads.jukebox
    local lines = {
        {'coins', tostring(data.coins)},
        {'bought', #data.hats > 0 and table.concat(data.hats, ', ') or 'nothing yet'},
        {'play time', string.format('%.1f seconds', data.playTime)},
        {'inputs seen by its event callback', tostring(data.inputs)},
        {'started on frame', tostring(data.startFrame)},
        {'require returns the same table', tostring(require('state.player-data') == data)},
        {'jukebox', jukebox and (jukebox.track .. ', beat ' .. jukebox.beats) or 'not added'},
    }
    for index, line in ipairs(lines) do
        local y = 20 + (index - 1) * 44
        graphics2d.drawText(nil, line[1], 40, y, {size = 28, color = sample.muted})
        graphics2d.drawText(nil, line[2], 520, y, {size = 28, color = sample.ink})
    end
    self.journal:draw(24, 350, area.height - 370, 26)
end

return Autoloads
