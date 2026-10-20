-- Launching apps: the test writes a small app package into the storage of the project and launches it with `haylen.launch`, which plays it in place of the project with a new engine. The small app tells that it was launched, and its Back button, its quit or the back button at its root starts the project again, as an app that downloads packages and plays them does.
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Journal = require('harness.journal')
local Test = require('harness.test')

local Launching = haylen.class('Launching', Test)

Launching.folder = 'launched-app'
Launching.manifest = {name = 'Launched App', identifier = 'com.haylen.launched', orientation = 'any', design = {width = 1280, height = 720, scaling = 'expand'}, clearColor = '#FF14324A'}
Launching.main = [[
local haylen = require('haylen')
local ui = require('haylen.ui')

-- Every way back quits this app, which starts the app that launched it again.
ui.mount(ui.column{align = 'center', gap = 24,
    ui.label{text = 'This app was launched by another app', font = 'title'},
    ui.label{text = haylen.launched() and 'Quitting it goes back to the app that launched it.' or 'Nothing launched it, so quitting closes it.'},
    ui.button{id = 'back', text = 'Back to the test project', variant = 'primary', onClick = function() haylen.quit() end},
})
]]

function Launching:enter()
    self.journal = Journal(10)
    self:frame{
        hint = 'Launch the small app, then press its Back button, or the back button of Android and Apple TV, to come back to the test project, which starts again from its menu.',
        focus = 'launch',
        controls = {
            ui.button{id = 'launch', text = 'Launch the small app', variant = 'primary', onClick = function() self:launch() end},
            ui.button{id = 'missing', text = 'Launch a package that is not there', onClick = function() self:launchMissing() end},
            ui.label{text = string.format('This app was launched by another app: %s.', haylen.launched() and 'yes' or 'no'), color = 'textMuted', font = 'caption'},
        },
        code = "storage.writeJson('launched-app/app.json', manifest)\nstorage.writeText('launched-app/source/main.lua', main)\nhaylen.launch(storage.root() .. '/launched-app')  -- Its quit comes back here.",
    }
end

-- Writes the package of the small app into the storage, where a downloaded package would land.
function Launching:launch()
    storage.writeJson(Launching.folder .. '/app.json', Launching.manifest)
    storage.writeText(Launching.folder .. '/source/main.lua', Launching.main)
    local path = storage.root() .. '/' .. Launching.folder
    self:log('Launching the package at "%s".', path)
    haylen.launch(path)
end

function Launching:launchMissing()
    local ok, message = pcall(haylen.launch, storage.root() .. '/no-such-app.zip')
    self.journal:add(ok and 'The missing package was launched.' or message, ok and Test.red or Test.green)
end

function Launching:draw(area)
    Test.caption('Answers', 24, 24, {color = Test.ink})
    self.journal:draw(24, 56, area.height - 76, 19)
end

return Launching
