-- An asset that no rule of "content.delivery" in "app.json" names is required: its bytes are on the device before the app starts, so "assets.json" reads it at once. The panel shows whether the app runs from its folder, as in development, or from a protected release, which ships the Lua modules as bytecode and every asset encrypted in shards.
local assets = require('haylen.assets')
local content = require('haylen.content')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local mapView = require('categories.delivery.map-view')

local Required = haylen.class('Required', Test)

Required.path = 'delivery/required/harbor.json'

function Required:enter()
    self.map = assets.json(Required.path)
    local stats = content.stats()
    local source = content.isProtected() and string.format('A protected release of generation %d, whose catalog lists %d files.', content.generation(), stats.catalogFiles) or 'The folder of the app, as in development. The command "haylen run samples/tests --config Release" plays the protected release instead, with the Lua modules as bytecode and every asset in encrypted shards.'
    self:frame{
        code = string.format("local map = assets.json('%s')\nprint(content.isLocal('%s'))", Required.path, Required.path),
        controls = {
            ui.sectionTitle{text = 'The asset'},
            ui.label{text = string.format('The map "%s" has %d rows and is on the device: %s.', self.map.name, #self.map.rows, tostring(content.isLocal(Required.path)))},
            ui.sectionTitle{text = 'The package'},
            ui.label{text = source, color = 'textMuted'},
            ui.label{text = string.format('Content status "%s".', content.status())},
        },
    }
end

function Required:draw(area)
    mapView.draw(self.map, area)
    Test.caption('The required map, read synchronously.', 16, 16)
end

return Required
