-- The base of every test: the frame with the Back button, the code, the title and the description, the region of the test with the play area the test draws in or its content, the card with the code of the test, an optional panel of controls, a status line and a line of hints with the keys that move around. The play area takes the keyboard, gamepads and remotes while it has the focus, and Tab, the View button or Play/Pause moves the focus to the controls and back. Escape, the B button and the Menu button of a TV remote go back to the test list.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local log = require('haylen.log')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local code = require('harness.code')
local navigation = require('harness.navigation')

local Test = haylen.class('Test', scene.Scene)

Test.panelWidth = 460
Test.stageColor = '#FF181D26'
Test.statusInterval = 0.2

-- The code card shows this many lines of code and scrolls the rest, so the region of every test has the same height.
Test.codeLines = 6
Test.codePadding = {16, 24}
Test.codeLineSpacing = 1.2

Test.ink = '#FFE8EAF2'
Test.muted = '#FF7A8099'
Test.line = '#FF2C3147'
Test.surface = '#FF232938'
Test.accent = '#FF8FB0FF'
Test.warm = '#FFF2B23A'
Test.green = '#FF6FDCA0'
Test.red = '#FFFF8A84'
Test.violet = '#FFC9A0FF'

-- The stage camera shows stage-local coordinates, where 0, 0 is the top-left corner of the play area wherever the layout puts it, unless the frame names a `view` to fit. A test keeps `entry`, `camera`, `view`, `gui`, `stage`, `area`, `statusTime`, `statusText`, `pendingStatus`, `codeSource`, `codeTheme` and `regionGuis` on itself for the harness, so its own fields and methods take other names, and `log`, `status`, `set`, `cancel`, `spawn` and `listen` stay the methods of the harness and of scenes, as do the scene hooks such as `load`, `pause`, `resume` and `event`, which the scene system calls.
function Test:init(entry)
    self.entry = entry
    self.statusTime = Test.statusInterval
    self.statusText = ''
    self.regionGuis = {}
    self.camera = graphics2d.newCamera()
    self.camera.anchor = 'topLeft'
end

-- Mounts the frame. The table `options` takes the `hint`, the `controls` of the panel and its `panelWidth`, `stage = false` for tests without a play area, `content`, nodes that take the place of the play area in a region that scrolls once they are taller than it, `fixed = true` for content that fills the region as it is and scrolls inside itself, such as a long collection, `code`, the Lua code the test runs, shown in a card under the play area or the content, `play = true` for tests that read the keyboard, gamepads or remotes as game input, which gives the play area the first focus, the id of another control to `focus` first, `view`, the width and height of a world centered on the origin that the camera fits into the play area, and `overlay`, nodes such as touch controls drawn over the region, which anchor to it with `anchorTo = 'parent'`.
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

    local main = {self:region(stage, options)}
    if options.code then
        main[2] = self:codeCard(options.code)
    end
    local middle = {ui.column{grow = 1, gap = 12, align = 'stretch', children = main}}
    if options.controls then
        middle[2] = ui.panel{id = 'panel', width = options.panelWidth or Test.panelWidth, align = 'stretch', ui.scroll{grow = 1, height = 0, ui.column{gap = 12, alignItems = 'stretch', children = options.controls}}}
    end

    local children = {
        ui.row{gap = 24, alignItems = 'start',
            ui.button{id = 'back', text = 'Back', onClick = function() self:cancel() end},
            ui.column{grow = 1, gap = 4,
                ui.row{gap = 16, minHeight = ui.themeMetric('controlHeight'), ui.badge{text = entry.code, tone = 'accent', solid = true}, ui.label{text = entry.title, font = 'heading', grow = 1}},
                ui.label{text = entry.description, color = 'textMuted'},
            },
        },
        ui.row{grow = 1, gap = 16, children = middle},
        ui.label{id = 'status', text = '', visible = false, color = 'accentText'},
        self:footer(options.hint, stage and options.controls ~= nil),
    }
    self.gui = ui.mount(ui.column{id = 'frame', padding = 24, gap = 12, onCancel = function() self:cancel() end, children = children}, {owner = self})
    self.gui:command(options.focus or (stage and options.play and 'stage') or 'back', 'focus')
    log.info(string.format('[%s] Started.', entry.code))
end

-- The region of the test fills the room between the header and the code card: the play area, rounded like the cards around it, with the overlays anchored to it, or the content in a scroll that fills the region and scrolls once the content is taller, or fixed content that fills it and scrolls inside itself, so nothing of the test ever runs under the card.
function Test:region(stage, options)
    local layers = {}
    if stage then
        layers[1] = ui.playArea{id = 'stage', align = 'stretch', radius = ui.themeMetric('panelRadius'), cornerColor = haylen.config.clearColor}
    elseif options.content and options.fixed then
        layers[1] = ui.column{id = 'content', gap = 12, align = 'stretch', children = options.content}
    elseif options.content then
        layers[1] = ui.scroll{id = 'content', fill = true, align = 'stretch', ui.column{gap = 12, children = options.content}}
    end
    for _, node in ipairs(options.overlay or {}) do
        layers[#layers + 1] = node
    end
    return ui.stack{id = 'region', grow = 1, align = 'stretch', children = layers}
end

-- The card holds the code in the monospace font with the colors of the theme, and keeps the height of its lines whatever the length of the code, which scrolls past them.
function Test:codeCard(source)
    self.codeSource = source
    self.codeTheme = ui.theme()
    local padding = Test.codePadding
    local height = math.ceil(ui.themeFont('monospace').size * Test.codeLineSpacing * Test.codeLines) + padding[1] * 2
    return ui.card{id = 'code-card', height = height, padding = padding, ui.scroll{grow = 1, height = 0, ui.richText{id = 'code', text = code.markup(source), font = 'monospace'}}}
end

-- The hint of the test, and the keys that move around the frame, shown the same way in every test.
function Test:footer(hint, switches)
    local keys = {}
    local function add(names, action)
        for _, name in ipairs(names) do
            keys[#keys + 1] = ui.badge{text = name}
        end
        keys[#keys + 1] = ui.label{text = action, font = 'caption', color = 'textMuted', margin = {0, 12, 0, 0}}
    end
    if switches then
        add({'Tab', 'View'}, 'Test and controls')
    end
    add({'Esc', 'B'}, 'Back to the list')
    return ui.row{gap = 24, alignItems = 'start',
        ui.label{text = hint or '', font = 'caption', color = 'textMuted', grow = 1},
        ui.row{gap = 6, children = keys},
    }
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
function Test:loadActions(actions)
    navigation.useActions(actions)
end

-- Keeps a GUI of the test inside its region, such as a bar or a HUD mounted on its own: the node `id` of the GUI, anchored to the safe area, takes the margins that keep it that far inside the region.
function Test:keepInRegion(gui, id, margin)
    self.regionGuis[#self.regionGuis + 1] = {gui = gui, id = id, margin = margin or 0, placed = nil}
end

-- Shows the last status, follows the code card through changes of theme, keeps the GUIs of the test inside its region and follows the play area, calling `resize` with the stage-local area whenever its size changes. Tests call it first from their own update.
function Test:update(dt)
    self.statusTime = self.statusTime + haylen.unscaledDelta()
    if not self.gui then
        return
    end
    if self.pendingStatus and self.statusTime >= Test.statusInterval then
        self:showStatus()
    end
    if self.codeSource and ui.theme() ~= self.codeTheme then
        self.codeTheme = ui.theme()
        self.gui:set('code', {text = code.markup(self.codeSource)})
    end
    self:placeRegionGuis()

    local stage = self.gui:bounds('stage')
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

-- The margins of a GUI kept in the region are the distances from the edges of the frame, which fills the safe area, to the edges of the region.
function Test:placeRegionGuis()
    local region = self.gui:bounds('region')
    local frame = self.gui:bounds('frame')
    if not region or not frame or #self.regionGuis == 0 then
        return
    end
    for _, entry in ipairs(self.regionGuis) do
        local margin = entry.margin
        local insets = {region.y - frame.y + margin, frame.x + frame.width - region.x - region.width + margin, frame.y + frame.height - region.y - region.height + margin, region.x - frame.x + margin}
        local key = table.concat(insets, ' ')
        if entry.gui.mounted and entry.placed ~= key then
            entry.placed = key
            entry.gui:set(entry.id, {margin = insets})
        end
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

-- Shows a line of live values a few times per second, so the numbers stay readable, and always the last line given, which shows once the interval passes. An empty line hides the status.
function Test:status(text)
    self.pendingStatus = text
    if self.statusTime >= Test.statusInterval then
        self:showStatus()
    end
end

function Test:showStatus()
    local text = self.pendingStatus
    self.pendingStatus = nil
    self.statusTime = 0
    if text ~= self.statusText then
        Test.setNode(self, 'status', {text = text})
    end
end

-- Sets properties of a node of the frame. A text given to the status shows the line, and an empty one hides it.
function Test:set(id, properties)
    Test.setNode(self, id, properties)
end

-- The harness calls it as a function of the class, since a test may give `set` another meaning.
function Test.setNode(test, id, properties)
    if id == 'status' and properties.text then
        test.statusText = properties.text
        properties.visible = properties.text ~= ''
    end
    test.gui:set(id, properties)
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
