-- An asset that no rule of "content.delivery" in "app.json" names is required: its bytes are on the device before the app starts, so "assets.json" reads it at once. The panel shows whether the app runs from its folder, as in development, or from a protected release, which ships the Lua modules as bytecode and every asset encrypted in shards.
local assets = require('haylen.assets')
local content = require('haylen.content')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local MapView = require('categories.delivery.map-view')

local Required = haylen.class('Required', Test)

Required.path = 'delivery/required/harbor.json'

function Required:enter()
    self.map = assets.json(Required.path)
    self.mapView = MapView()
    local stats = content.stats()
    local source = content.isProtected() and string.format('A protected release of generation %d, whose catalog lists %d files.', content.generation(), stats.catalogFiles) or 'The folder of the app, as in development. The command "haylen run samples/tests --config Release" plays the protected release instead, with the Lua modules as bytecode and every asset in encrypted shards.'
    local onDevice = content.isLocal(Required.path)
    self:frame{
        code = string.format("local map = assets.json('%s')\nprint(content.isLocal('%s'))", Required.path, Required.path),
        controls = {
            ui.sectionTitle{text = 'The asset'},
            ui.label{text = string.format('The map "%s", %d rows of tiles.', self.map.name, #self.map.rows)},
            ui.row{gap = 12, alignItems = 'center',
                ui.badge{text = onDevice and 'On the device' or 'Not on the device', tone = onDevice and 'success' or 'danger', solid = true},
                ui.badge{text = 'Read synchronously', tone = 'information'},
            },
            ui.sectionTitle{text = 'The package'},
            ui.label{text = source, color = 'textMuted'},
            ui.row{gap = 12, alignItems = 'center',
                ui.label{text = 'Content status', color = 'textMuted'},
                ui.badge{text = (content.status():gsub('^%l', string.upper)), tone = 'neutral'},
            },
        },
    }
end

function Required:draw(area)
    self.mapView:draw(self.map, area)
    Test.caption('The harbor, a required map read at once.', area.width / 2, 20, {size = 22, color = Test.ink, anchor = {0.5, 0}})
end

return Required
