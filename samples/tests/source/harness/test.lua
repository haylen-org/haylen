-- The base of every test: the frame with the Back button, the code, the title and the description, the play area the test draws in, an optional panel of controls, a status line and a line of hints. The play area takes the keyboard, gamepads and remotes while it has the focus, and Tab, the View button or Play/Pause moves the focus to the controls and back. Escape, the B button and the Menu button of a TV remote go back to the test list.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local log = require('haylen.log')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local navigation = require('harness.navigation')

local Test = haylen.class('Test', scene.Scene)

Test.panelWidth = 460
Test.stageColor = '#FF181D26'
Test.statusInterval = 0.2
Test.backHint = 'Escape, the B button or Back returns to the list.'
Test.focusHint = 'Tab walks the controls and the test, and the View button or Play/Pause moves the focus between the test and its controls.'

Test.ink = '#FFE8EAF2'
Test.muted = '#FF7A8099'
Test.line = '#FF2C3147'
Test.surface = '#FF232938'
Test.accent = '#FF8FB0FF'
Test.warm = '#FFF2B23A'
Test.green = '#FF6FDCA0'
Test.red = '#FFFF8A84'
Test.violet = '#FFC9A0FF'

-- The stage camera shows stage-local coordinates, where 0, 0 is the top-left corner of the play area wherever the layout puts it, unless the frame names a `view` to fit. A test keeps `entry`, `camera`, `view`, `document`, `stage`, `area` and `statusTime` on itself for the harness, so its own fields and methods take other names, and `log`, `status`, `set`, `cancel`, `spawn` and `listen` stay the methods of the harness and of scenes, as do the scene hooks such as `load`, `pause`, `resume` and `event`, which the scene system calls.
function Test:init(entry)
    self.entry = entry
    self.statusTime = Test.statusInterval
    self.camera = graphics2d.newCamera()
    self.camera.anchor = 'topLeft'
end

-- Mounts the frame. The table `options` takes the `hint`, the `controls` of the panel and its `panelWidth`, `stage = false` for tests without a play area, `content`, nodes that take the place of the play area, `code`, the Lua code the test runs, shown in a card under the play area, `play = true` for tests that read the keyboard, gamepads or remotes as game input, which gives the play area the first focus, the id of another control to `focus` first, `view`, the width and height of a world centered on the origin that the camera fits into the play area, and `overlay`, nodes such as touch controls drawn over the play area.
function Test:frame(options)
    options = options or {}
    local entry = self.entry
    navigation.current = entry
    window.setBackLeavesApp(false)

    local stage = options.stage ~= false and not options.content
    self.view = options.view
    if self.view then
        self.camera.anchor = 'center'
    end

    local middle = {ui.spacer{grow = 1}}
    if stage and options.code then
        middle[1] = ui.column{grow = 1, gap = 12, align = 'stretch',
            ui.playArea{id = 'stage', grow = 1, align = 'stretch'},
            ui.card{ui.label{id = 'code', text = options.code, font = 'monospace'}},
        }
    elseif stage then
        middle[1] = ui.playArea{id = 'stage', grow = 1, align = 'stretch'}
    elseif options.content then
        middle[1] = ui.column{grow = 1, align = 'stretch', gap = 12, children = options.content}
    end
    if options.controls then
        middle[2] = ui.panel{id = 'panel', width = options.panelWidth or Test.panelWidth, align = 'stretch', ui.scroll{grow = 1, height = 0, ui.column{gap = 12, children = options.controls}}}
    end
    local hint = options.hint and options.hint .. ' ' or ''
    if stage and options.controls then
        hint = hint .. Test.focusHint .. ' '
    end

    local children = {
        ui.row{gap = 24, align = 'start',
            ui.button{id = 'back', text = 'Back', onClick = function() self:cancel() end},
            ui.column{grow = 1, gap = 4,
                ui.row{gap = 16, ui.badge{text = entry.code, tone = 'accent', solid = true}, ui.label{text = entry.title, font = 'heading', grow = 1}},
                ui.label{text = entry.description, color = 'textMuted'},
            },
        },
        ui.row{grow = 1, gap = 16, children = middle},
        ui.label{id = 'status', text = '', font = 'monospace', color = 'accentText'},
        ui.label{text = hint .. Test.backHint, font = 'caption', color = 'textMuted'},
    }
    for _, node in ipairs(options.overlay or {}) do
        children[#children + 1] = node
    end

    self.document = ui.mount(ui.column{padding = 24, gap = 12, onCancel = function() self:cancel() end, children = children}, {owner = self})
    self.document:command(options.focus or (stage and options.play and 'stage') or 'back', 'focus')
    log.info(string.format('[%s] Started.', entry.code))
end

-- Leaves the frame with the action map of the project. Tests that override `exit` call it.
function Test:exit()
    if navigation.current == self.entry then
        navigation.current = nil
    end
    navigation.useActions(nil)
end

-- Goes back to the test list. Tests that give the cancel buttons a meaning of their own override it.
function Test:cancel()
    navigation.back()
end

-- Loads the action map of the test, which the project keeps with the action that moves the focus between the test and its controls, until the test exits.
function Test:loadActions(document)
    navigation.useActions(document)
end

-- Follows the play area, calling `resize` with the stage-local area whenever its size changes. Tests call it first from their own update.
function Test:update(dt)
    self.statusTime = self.statusTime + haylen.unscaledDelta()

    local stage = self.document and self.document:bounds('stage')
    if not stage then
        return
    end
    self.stage = stage
    self.camera.viewport = stage
    if self.view then
        local zoom = math.min(stage.width / self.view[1], stage.height / self.view[2])
        self.camera.zoom = {zoom, zoom}
    end
    if not self.area or self.area.width ~= stage.width or self.area.height ~= stage.height then
        self.area = m.rect(0, 0, stage.width, stage.height)
        self:resize(self.area)
    end
end

function Test:resize(area)
end

-- Draws the stage background and lets the test draw in the coordinates of its camera once the play area has a size.
function Test:render()
    if not self.area then
        return
    end
    graphics2d.beginWorld(self.camera)
    graphics2d.drawRect(graphics2d.canvasBounds(), Test.stageColor, {layer = -1000})
    self:draw(self.area)
end

function Test:draw(area)
end

-- Converts a point of the screen, such as the pointer or a finger, to the coordinates of the stage camera.
function Test:toStage(x, y)
    return self.camera:screenToWorld(x, y)
end

-- Shows a line of live values a few times per second, so the numbers stay readable.
function Test:status(text)
    if self.statusTime >= Test.statusInterval then
        self.statusTime = 0
        self.document:set('status', {text = text})
    end
end

function Test:set(id, properties)
    self.document:set(id, properties)
end

-- Writes a line to the log with the code of the test in front of it.
function Test:log(format, ...)
    log.info(string.format('[%s] ' .. format, self.entry.code, ...))
end

-- Draws a small caption, the way every test labels what it draws.
function Test.caption(text, x, y, options)
    options = options or {}
    graphics2d.drawText(nil, text, x, y, {size = options.size or 19, color = options.color or Test.muted, anchor = options.anchor or {0, 0}, maxWidth = options.maxWidth or 0, layer = options.layer or 2})
end

return Test
