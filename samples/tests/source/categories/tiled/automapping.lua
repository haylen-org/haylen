-- Automapping: a garden painted with plain grass, dirt and water gets its shores, its decoration and its slimes from the rules of a rule map, the way Tiled automaps it, with a new seed on every press, and a toggle that shows the plain map again.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')

local Automapping = haylen.class('Automapping', MapTest)

Automapping.code = [[
local rules = assets.load('tiled/rules/rules.txt', 'tiledRules', {filter = 'linear'})
local map = tiled.newMapRenderer(assets.load('tiled/maps/garden.tmj', nil, {filter = 'linear'}))
local matches = map:automap(rules, {seed = 7})  -- The same seed gives the same garden.
map:draw(camera)]]

Automapping.mapPath = 'tiled/maps/garden.tmj'
Automapping.rulesPath = 'tiled/rules/rules.txt'

function Automapping:enter()
    self.rules = assets.load(Automapping.rulesPath, 'tiledRules', {filter = 'linear'})
    self.seed = 1
    self.plain = false
    self:automap()
    self:frame{
        hint = 'The rules turn grass beside water into sand, scatter bushes, rocks and flowers over open grass and put a slime on the path now and then. R or the X button automaps again with a new seed.',
        code = Automapping.code,
        controls = {
            ui.button{id = 'again', text = 'Automap with a new seed', onClick = function() self:next() end},
            ui.toggle{id = 'plain', text = 'Show the plain map', onChange = function(event) self.plain = event.checked end},
        },
        view = {32 * 32 + 64, 18 * 32 + 64},
        focus = 'again',
    }
    self.camera:snapTo(32 * 32 / 2, 18 * 32 / 2)
end

-- Each run starts from the map as it was painted, so the seed alone decides the result.
function Automapping:automap()
    self.source = Automapping.loadMap(Automapping.mapPath)
    self.map = Automapping.loadMap(Automapping.mapPath)
    self.matches = self.map:automap(self.rules, {seed = self.seed})
    self.slimes = #self.map:objects('actors')
end

function Automapping:next()
    self.seed = self.seed + 1
    self:automap()
end

function Automapping:update(dt)
    Automapping.super.update(self, dt)
    if input.pressed('reset') then
        self:next()
    end
    self:status(string.format('Seed %d, %d rule matches, %d slimes, rule maps %d, draw calls %d', self.seed, self.matches, self.slimes, #self.rules.ruleMaps, graphics2d.stats().drawCalls))
end

function Automapping:draw(area)
    if self.plain then
        self.source:draw(self.camera)
    else
        self.map:draw(self.camera)
    end
end

return Automapping
