-- Smart Home: the control panel of a home on a wall or a tablet, with the house under the sky of the hour, the scenes, the climate, the lights of every room, the security with its camera, the energy of the day and the devices, all changing as the day goes on.
local haylen = require('haylen')
local scene = require('haylen.scene')

local Style = require('style')

Style.registerTheme()
scene.push(require('scenes.wall')())

if haylen.platform == 'headless' then
    require('tour').start()
end
