-- Photo Social: a made-up photo sharing app with a feed of posts with likes, comments and captions, stories that play full screen, profiles with a grid of pictures and a follow button, a new post with a picture, a crop, a filter and a caption, and direct messages, in a bright white look of rounded prints with a blue to teal ring and a blue accent.
local haylen = require('haylen')
local input = require('haylen.input')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Style = require('style')

Style.register()

local Photos = require('photos')
local Shell = require('shell')
local Tour = require('tour')

-- The arrows, the directional pad and the left stick step through the stories while their picture has the focus.
input.defineAction({name = 'storyPrevious', type = 'button', bindings = {'key:left', 'button:dpadLeft', 'axis:leftX-'}})
input.defineAction({name = 'storyNext', type = 'button', bindings = {'key:right', 'button:dpadRight', 'axis:leftX+'}})

-- The interface keeps its size in points on every screen, and a television, seen from the couch, draws it half as large again.
if system.info().deviceKind == 'tv' then
    ui.setScale(1.5)
end
ui.setTheme('photos')
Photos.start()
Shell.start()

if haylen.platform == 'headless' then
    Tour.start()
end
