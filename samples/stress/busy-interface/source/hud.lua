-- The panel beside the dashboard: the title, a readout of the frame and of the counts of the app, refreshed a few times per second, and the controls that raise and lower the load.
local debugging = require('haylen.debug')
local ui = require('haylen.ui')

local Hud = {}
Hud.__index = Hud

Hud.interval = 0.25
Hud.width = 430
Hud.frameRows = {
    {id = 'frameTime', label = 'Last frame'},
    {id = 'averageTime', label = 'Average frame'},
    {id = 'lowTime', label = 'One percent low'},
    {id = 'frameRate', label = 'Frames per second'},
    {id = 'drawCalls', label = 'Draw calls'},
}

function Hud.count(value)
    local digits = tostring(math.floor(value)):reverse():gsub('(%d%d%d)', '%1,'):reverse()
    return (digits:gsub('^,', ''))
end

function Hud.milliseconds(value)
    return string.format('%.2f ms', value)
end

local function readoutRow(row)
    return ui.row{gap = 12,
        ui.label{text = row.label, font = 'caption', color = 'textMuted', grow = 1, wrap = false},
        ui.label{id = row.id, text = '', font = 'caption', textAlign = 'end', wrap = false},
    }
end

-- Mounts the `content` and the panel beside it for `owner`. The table `options` takes the `content` node, the `title`, the `caption`, the `rows` of the counts of the app as `{id, label}` tables, the `controls`, a `hint` and the id of the control to `focus` first.
function Hud.new(owner, options)
    local self = setmetatable({elapsed = Hud.interval, shown = {}, changes = 0}, Hud)
    local readout = {}
    for _, row in ipairs(Hud.frameRows) do
        readout[#readout + 1] = readoutRow(row)
    end
    for _, row in ipairs(options.rows) do
        readout[#readout + 1] = readoutRow(row)
    end

    local panel = {
        ui.label{text = options.title, font = 'heading'},
        ui.label{text = options.caption, font = 'caption', color = 'textMuted'},
        ui.divider{},
        ui.column{gap = 2, children = readout},
        ui.divider{},
    }
    for _, control in ipairs(options.controls) do
        panel[#panel + 1] = control
    end
    panel[#panel + 1] = ui.label{text = options.hint, font = 'caption', color = 'textMuted'}

    self.gui = ui.mount(ui.row{padding = 16, gap = 16,
        options.content,
        ui.panel{id = 'panel', width = Hud.width, align = 'stretch', ui.scroll{grow = 1, height = 0, ui.column{gap = 16, children = panel}}},
    }, {owner = owner})
    self.gui:command(options.focus, 'focus')
    return self
end

-- Changes a property of a node only when its value differs from the one it shows, counts the change and returns whether it made one.
function Hud:set(id, key, value)
    local shownKey = id .. '.' .. key
    if self.shown[shownKey] == value then
        return false
    end
    self.shown[shownKey] = value
    self.gui:set(id, {[key] = value})
    self.changes = self.changes + 1
    return true
end

-- Forgets what every node shows, such as after the nodes were built again.
function Hud:forget()
    self.shown = {}
end

-- Returns the changes made since the last call.
function Hud:takeChanges()
    local changes = self.changes
    self.changes = 0
    return changes
end

function Hud:show(id, text)
    self:set(id, 'text', text)
end

-- Counts the time since the last refresh and, once the interval passed, shows the frame numbers and returns `true`, so the app refreshes its own rows.
function Hud:update(dt)
    self.elapsed = self.elapsed + dt
    if self.elapsed < Hud.interval then
        return false
    end
    self.elapsed = 0
    local stats = debugging.stats()
    local frame = stats.frame
    self:show('frameTime', Hud.milliseconds(frame.milliseconds))
    self:show('averageTime', Hud.milliseconds(frame.average))
    self:show('lowTime', Hud.milliseconds(frame.onePercentLow))
    self:show('frameRate', string.format('%.0f', frame.fps))
    self:show('drawCalls', Hud.count(stats.rendering.drawCalls))
    self.stats = stats
    return true
end

-- Returns the milliseconds the profiler scope `name` took in the last frame.
function Hud.scope(name)
    for _, scope in ipairs(debugging.frame().scopes) do
        if scope.name == name then
            return scope.milliseconds
        end
    end
    return 0
end

return Hud
