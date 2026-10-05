-- Autoloads: the player data that `app.json` loads before `main.lua`, shared by this test, a shop scene and the coin counter it draws while an events test shows, and a jukebox added at run time with `haylen.autoload`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local EventsTest = require('categories.events.events-test')
local Journal = require('harness.journal')
local Overlay = require('categories.events.overlay')
local Test = require('harness.test')

local Autoloads = haylen.class('Autoloads', EventsTest)

local kCode = [[
"autoload": ["categories.events.player-data"]  -- In `app.json`, loaded before `source/main.lua`.
local playerData = haylen.autoloads.playerData  -- The same table as `require('categories.events.player-data')`.
haylen.autoload('categories.events.jukebox')  -- One more autoload at run time, named `jukebox`.]]

local kItems = {{id = 'hat', text = 'Buy a hat for 25', price = 25}, {id = 'scarf', text = 'Buy a scarf for 15', price = 15}}

-- The shop spends the coins of the same player data the test shows.
local Shop = haylen.class('Shop', Overlay)

function Shop:enter(params)
    local data, journal = haylen.autoloads.playerData, params.journal
    local children = {ui.label{id = 'coins', text = data.coins .. ' coins', color = 'warningText'}}
    for _, item in ipairs(kItems) do
        children[#children + 1] = ui.button{id = item.id, text = item.text, onClick = function()
            if data:spend(item.price) then
                table.insert(data.hats, item.id)
                journal:add('The shop sold a ' .. item.id .. ', ' .. data.coins .. ' coins left', Test.green)
            else
                journal:add('Not enough coins for a ' .. item.id, Test.red)
            end
            self.gui:set('coins', {text = data.coins .. ' coins'})
        end}
    end
    self:card('The shop', children)
end

function Autoloads:enter()
    self.journal = Journal()
    self:listen('autoloadStarted', function(info) self.journal:add('Event "autoloadStarted" for "' .. info.name .. '"', Test.accent) end)
    self:frame({
        hint = 'Earn coins here and spend them in the shop. The autoload draws the counter in the corner while an events test shows.',
        code = kCode,
        controls = {
            ui.button{id = 'earn', text = 'Earn 10 coins', variant = 'primary', onClick = function()
                haylen.autoloads.playerData:earn(10)
                self.journal:add('Earned 10 coins')
            end},
            ui.button{id = 'shop', text = 'Open the shop', onClick = function() Shop():open({journal = self.journal}) end},
            ui.button{id = 'jukebox', text = 'Add the jukebox autoload', enabled = haylen.autoloads.jukebox == nil, onClick = function() self:addJukebox() end},
        },
        focus = 'earn',
    })
end

function Autoloads:addJukebox()
    haylen.autoload('categories.events.jukebox')
    self:set('jukebox', {enabled = false})
end

function Autoloads:update(dt)
    Autoloads.super.update(self, dt)
    local names = {}
    for name in pairs(haylen.autoloads) do
        names[#names + 1] = name
    end
    table.sort(names)
    self:status('Autoloads in "haylen.autoloads": ' .. table.concat(names, ', '))
end

function Autoloads:draw(area)
    local data, jukebox = haylen.autoloads.playerData, haylen.autoloads.jukebox
    local lines = {
        {'Coins', tostring(data.coins)},
        {'Bought', #data.hats > 0 and table.concat(data.hats, ', ') or 'nothing yet'},
        {'Play time', string.format('%.1f seconds', data.playTime)},
        {'Inputs seen by its event callback', tostring(data.inputs)},
        {'Started on frame', tostring(data.startFrame)},
        {'The call "require" returns the same table', string.format('"%s"', require('categories.events.player-data') == data)},
        {'Jukebox', jukebox and (jukebox.track .. ', beat ' .. jukebox.beats) or 'not added'},
    }
    for index, line in ipairs(lines) do
        local y = 20 + (index - 1) * 44
        graphics2d.drawText(nil, line[1], 40, y, {size = 28, color = Test.muted})
        graphics2d.drawText(nil, line[2], 640, y, {size = 28, color = Test.ink})
    end
    self.journal:draw(24, 350, area.height - 370, 26)
end

return Autoloads
