-- Car Dashboard: the screen of an electric car on a drive through the city, with the speedometer, the road ahead with the lane assist, the navigation with a moving map, the battery and its range, the energy use, the tyres, the driver assistants, the climate and the dock, all drawn with the shapes, paths, gradients and text of the engine and animated as the drive goes on.
local haylen = require('haylen')
local scene = require('haylen.scene')

local Style = require('style')

Style.registerTheme()
scene.push(require('scenes.cockpit')())

if haylen.platform == 'headless' then
    require('tour').start()
end
