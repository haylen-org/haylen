-- Autoloads: the player data that `app.json` loads before `main.lua`, shared by this test, a shop scene and the coin counter it draws while an events test shows, and a jukebox added at run time with `haylen.autoload`.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local EventsTest = require('categories.events.events-test')
local Journal = require('harness.journal')
local Overlay = require('categories.events.overlay')
local Test = require('harness.test')

local Autoloads = haylen.class('Autoloads', EventsTest)

local kCode = [[
-- The file "app.json" lists "categories.events.player-data" in "autoload".
local playerData = haylen.autoloads.playerData
playerData:earn(10)
haylen.autoload('categories.events.jukebox')  -- Named "jukebox".
print(haylen.autoloads.jukebox.beats)]]

local kItems = {{id = 'hat', text = 'Buy a hat for 25', price = 25}, {id = 'scarf', text = 'Buy a scarf for 15', price = 15}}
local kRowHeight = 50

-- The shop spends the coins of the same player data the test shows.
local Shop = haylen.class('Shop', Overlay)

function Shop:enter(params)
    local data, journal = haylen.autoloads.playerData, params.journal
    local children = {ui.label{id = 'coins', text = data.coins .. ' coins', color = 'warningText'}}
    for _, item in ipairs(kItems) do
        children[#children + 1] = ui.button{id = item.id, text = item.text, onClick = function()
            if data:spend(item.price) then
                table.insert(data.hats, item.id)
                journal:add('The shop sold a ' .. item.id .. ', and ' .. data.coins .. ' coins are left.', Test.green)
            else
                journal:add('There are not enough coins for a ' .. item.id .. '.', Test.red)
            end
            self.gui:set('coins', {text = data.coins .. ' coins'})
        end}
    end
    self:card('The shop', children)
end

function Autoloads:enter()
    self.journal = Journal()
    self.coin = assets.texture('physics/coin.png', {filter = 'linear'})
    self:listen('autoloadStarted', function(info) self.journal:add('The event "autoloadStarted" names "' .. info.name .. '".', Test.accent) end)
    self:frame({
        hint = 'Earn coins here and spend them in the shop. The autoload draws the counter in the corner while an events test shows.',
        code = kCode,
        controls = {
            ui.button{id = 'earn', text = 'Earn 10 coins', variant = 'primary', onClick = function()
                haylen.autoloads.playerData:earn(10)
                self.journal:add('The player earns 10 coins.')
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
        names[#names + 1] = '"' .. name .. '"'
    end
    table.sort(names)
    self:status('The table "haylen.autoloads" holds ' .. table.concat(names, ' and ') .. '.')
end

-- Formats seconds as minutes and seconds for people.
function Autoloads.duration(seconds)
    local minutes = math.floor(seconds / 60)
    if minutes == 0 then
        return string.format('%d s', seconds)
    end
    return string.format('%d min %02d s', minutes, seconds % 60)
end

-- Draws the rows of a card as names and values in two columns over alternating stripes.
function Autoloads:drawRows(rows, rect)
    local top = rect[2] + 72
    for index, row in ipairs(rows) do
        local y = top + (index - 1) * kRowHeight
        if index % 2 == 1 then
            graphics2d.drawShape({rect[1] + 12, y, rect[3] - 24, kRowHeight}, {radius = 10, color = '#FF1E2330', layer = 1})
        end
        graphics2d.drawText(nil, row[1], rect[1] + 28, y + kRowHeight / 2, {size = 24, color = Test.muted, anchor = {0, 0.5}, layer = 2})
        if row[2] ~= '' then
            graphics2d.drawText(nil, row[2], rect[1] + rect[3] * 0.56, y + kRowHeight / 2, {size = 24, color = Test.ink, anchor = {0, 0.5}, layer = 2})
        end
    end
end

function Autoloads:drawPlayerData(rect)
    local data = haylen.autoloads.playerData
    EventsTest.panel(rect)
    self:label('Player data', rect[1] + 28, rect[2] + 36, {anchor = {0, 0.5}, size = 30})
    local rows = {
        {'Coins', ''},
        {'Bought in the shop', #data.hats > 0 and '' or 'Nothing yet'},
        {'Play time', Autoloads.duration(math.floor(data.playTime))},
        {'Inputs its event hook saw', tostring(data.inputs)},
        {'Started on frame', tostring(data.startFrame)},
        {'The call "require" gives the same table', require('categories.events.player-data') == data and 'Yes' or 'No'},
    }
    self:drawRows(rows, rect)
    local valueX = rect[1] + rect[3] * 0.56
    local top = rect[2] + 72
    graphics2d.draw(self.coin, valueX + 20, top + kRowHeight / 2, {width = 40, height = 40, layer = 3})
    graphics2d.drawText(nil, tostring(data.coins), valueX + 50, top + kRowHeight / 2, {size = 24, color = Test.ink, anchor = {0, 0.5}, layer = 3})
    for index, item in ipairs(data.hats) do
        if index <= 6 then
            self:icon(item, valueX + 20 + (index - 1) * 48, top + kRowHeight * 1.5, 44, nil, 3)
        end
    end
end

function Autoloads:drawJukebox(rect)
    local jukebox = haylen.autoloads.jukebox
    EventsTest.panel(rect, jukebox and (jukebox.time * jukebox.tempo / 60) % 1 < 0.2 and 0.6 or 0)
    self:label('Jukebox', rect[1] + 28, rect[2] + 36, {anchor = {0, 0.5}, size = 30})
    local beat = jukebox and 1 + 0.08 * math.max(0, 1 - (jukebox.time * jukebox.tempo / 60) % 1 * 4) or 1
    local size = math.min(rect[3] * 0.5, rect[4] * 0.42) * beat
    local x, y = rect[1] + rect[3] / 2, rect[2] + rect[4] * 0.45
    self:icon('jukebox', x, y, size, not jukebox and '#55FFFFFF' or nil)
    local text = jukebox and string.format('%s, beat %d', jukebox.track, jukebox.beats) or 'Not added yet'
    graphics2d.drawText(nil, text, x, rect[2] + rect[4] - 40, {size = 24, color = jukebox and Test.ink or Test.muted, anchor = {0.5, 0.5}, layer = 2})
end

function Autoloads:draw(area)
    local margin, gap = 24, 16
    local height = 72 + kRowHeight * 6 + 16
    local width = area.width - margin * 2
    self:drawPlayerData({margin, margin, width * 0.66 - gap / 2, height})
    self:drawJukebox({margin + width * 0.66 + gap / 2, EventsTest.counterRoom, width * 0.34 - gap / 2, height - EventsTest.counterRoom + margin})
    local top = margin + height + gap
    self:logPanel(self.journal, {margin, top, width, area.height - top - margin}, 'What happened', {'trophy', 'Earn coins here and spend them in the shop.'})
end

return Autoloads
