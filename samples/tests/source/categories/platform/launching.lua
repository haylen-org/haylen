-- Launching apps: the test writes a small app package into the storage of the project and launches it with `haylen.launch`, which plays it in place of the project with a new engine. The small app tells that it was launched, and its Back button, its quit or the back button at its root starts the project again, as an app that downloads packages and plays them does.
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

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
    self.lines = {}
    self.logged = 0
    local manifest = Launching.manifest
    self:frame{
        hint = 'Launch the small app, then press its Back button, or the back button of Android and Apple TV, to come back to the test project, which starts again from its menu.',
        focus = 'launch',
        controls = {
            ui.button{id = 'launch', text = 'Launch the small app', variant = 'primary', onClick = function() self:launch() end},
            ui.button{id = 'missing', text = 'Launch a missing package', onClick = function() self:launchMissing() end},
            ui.label{text = haylen.launched() and 'Another app launched this project, so quitting it goes back there.' or 'Nothing launched this project, so its own quit closes it.', color = 'textMuted', font = 'caption'},
        },
        content = {ui.row{grow = 1, gap = 24, align = 'stretch',
            ui.panel{grow = 1, align = 'stretch', gap = 16,
                ui.sectionTitle{text = 'The small app'},
                ui.stack{height = 300, align = 'stretch',
                    ui.card{align = 'stretch', style = {colors = {raised = manifest.clearColor}}},
                    ui.column{align = 'center', justify = 'center', gap = 14,
                        ui.label{text = 'This app was launched by another app', font = 'heading', textAlign = 'center'},
                        ui.label{text = 'Quitting it goes back to the app that launched it.', color = 'textMuted', textAlign = 'center'},
                        ui.button{text = 'Back to the test project', variant = 'primary', focusable = false},
                    },
                },
                ui.label{text = string.format('"%s", "%s", %d by %d in any orientation.', manifest.name, manifest.identifier, manifest.design.width, manifest.design.height), color = 'textMuted'},
                ui.column{gap = 8,
                    ui.row{gap = 12, ui.icon{image = 'plugins/file.svg', size = 36, color = 'accentText'}, ui.label{text = Launching.folder .. '/app.json', font = 'monospace'}},
                    ui.row{gap = 12, ui.icon{image = 'plugins/file.svg', size = 36, color = 'accentText'}, ui.label{text = Launching.folder .. '/source/main.lua', font = 'monospace'}},
                },
            },
            ui.panel{width = 600, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Answers'},
                ui.column{id = 'answersEmpty', grow = 1, gap = 16, justify = 'center', align = 'stretch',
                    ui.icon{image = 'plugins/link.svg', size = 96, color = 'textMuted', align = 'center'},
                    ui.label{text = 'Nothing launched yet. A launched app starts at once in place of the project, and a missing package answers here.', color = 'textMuted', textAlign = 'center', align = 'center', maxWidth = 440},
                },
                ui.scroll{id = 'answersScroll', grow = 1, height = 0, focusable = false, visible = false, ui.list{id = 'answers', items = {}}},
            },
        }},
        code = "storage.writeJson('launched-app/app.json', manifest)\nstorage.writeText('launched-app/source/main.lua', main)\nhaylen.launch(storage.root() .. '/launched-app')  -- Its quit comes back here.",
    }
end

function Launching:record(text, detail)
    self.logged = self.logged + 1
    table.insert(self.lines, 1, {id = 'line-' .. self.logged, text = text, caption = os.date('%H:%M:%S') .. '  ' .. detail})
    self:set('answers', {items = self.lines})
    self:set('answersEmpty', {visible = false})
    self:set('answersScroll', {visible = true})
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
    self:record(ok and 'The missing package launched' or 'The launch refused the missing package', ok and 'Nothing should have launched.' or message)
end

return Launching
