-- The app lays out in design units, and the scaling policy of `app.json` maps them onto the screen. The preview applies each policy to screens of other shapes, and the live values show how this app is mapped right now, in either orientation.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Device = require('categories.safe-area.device')
local Test = require('harness.test')

local Scaling = haylen.class('Scaling', Test)

Scaling.code = [[
-- The file `app.json` sets `"design": {"width": 1920, "height": 1080, "scaling": "expand"}`.
viewport.setScaling('fit')
local width, height = viewport.designSize()
local visible = viewport.visibleRect()  -- Larger than the design area with `expand`.
local unitX, unitY = viewport.pixelsPerUnit()]]

Scaling.policies = {{id = 'fit', text = 'Fit'}, {id = 'fill', text = 'Fill'}, {id = 'stretch', text = 'Stretch'}, {id = 'expand', text = 'Expand'}, {id = 'pixelPerfect', text = 'Pixel perfect'}, {id = 'fitWidth', text = 'Fit width'}, {id = 'none', text = 'None'}}
Scaling.screens = {
    {id = 'phone-portrait', text = 'Phone in portrait', width = 1170, height = 2532, device = 'island'},
    {id = 'phone-landscape', text = 'Phone in landscape', width = 2532, height = 1170, device = 'island', sideways = true},
    {id = 'tablet', text = 'Tablet', width = 2048, height = 1536, device = 'tablet', sideways = true},
    {id = 'ultrawide', text = 'Ultrawide monitor', width = 3440, height = 1440, device = 'tv'},
    {id = 'full-hd', text = 'Full HD monitor', width = 1920, height = 1080, device = 'tv'},
    {id = 'small', text = 'Small window', width = 800, height = 600, device = 'tv'},
}

function Scaling:init(entry)
    Scaling.super.init(self, entry)
    self.policy = viewport.scaling()
    self.screen = Scaling.screens[1]
    self.scenery = assets.texture('interface/images/landscape.png', {filter = 'linear'})
    self.heart = assets.texture('interface/themes/fantasy/icons/heart.png', {filter = 'linear'})
    self.coin = assets.texture('interface/themes/fantasy/icons/coin.png', {filter = 'linear'})
end

function Scaling:enter()
    window.lockOrientation('any')
    local screens = {}
    for index, screen in ipairs(Scaling.screens) do
        screens[index] = {id = screen.id, text = string.format('%s, %d x %d', screen.text, screen.width, screen.height)}
    end
    self:frame{
        hint = 'Left and right change the focused stepper. The policy is "design.scaling" in "app.json", and this project uses "expand". The preview draws the design area of 1920 by 1080 on a simulated screen: black bars are letterboxing, the striped band is the extra visible area of "expand", and whatever leaves the screen is cropped.',
        code = Scaling.code,
        focus = 'policy',
        panelWidth = 760,
        controls = {
            ui.sectionTitle{text = 'Preview'},
            ui.stepper{id = 'policy', items = Scaling.policies, selected = self.policy, wrap = true, onChange = function(event) self.policy = event.value end},
            ui.stepper{id = 'screen', items = screens, selected = self.screen.id, wrap = true, onChange = function(event)
                for _, screen in ipairs(Scaling.screens) do
                    if screen.id == event.value then
                        self.screen = screen
                    end
                end
            end},
            ui.table{id = 'mapping', columns = {{text = 'Preview', width = 170}, {text = 'Value'}}},
            ui.sectionTitle{text = 'This app now'},
            ui.table{id = 'live', columns = {{text = 'Measure', width = 170}, {text = 'Value'}}},
        },
    }
end

function Scaling:exit()
    window.lockOrientation('landscape')
    Scaling.super.exit(self)
end

-- Maps the design area onto a screen of `width` by `height` pixels the way the engine does, and returns the scale on each axis, the pixel offset of the design origin and the visible design width and height.
function Scaling:map(width, height)
    local designWidth, designHeight = viewport.designSize()
    local ratioX, ratioY = width / designWidth, height / designHeight
    local policy = self.policy
    if policy == 'stretch' then
        return ratioX, ratioY, 0, 0, designWidth, designHeight
    end
    if policy == 'none' then
        local points = window.dpiScale()
        return points, points, 0, 0, width / points, height / points
    end
    local scale = math.min(ratioX, ratioY)
    if policy == 'fitWidth' then
        scale = ratioX
    elseif policy == 'fill' then
        scale = math.max(ratioX, ratioY)
    elseif policy == 'pixelPerfect' then
        scale = scale >= 1 and math.floor(scale) or 1 / math.ceil(math.max(designWidth / width, designHeight / height))
    end
    local offsetX, offsetY = (width - designWidth * scale) / 2, (height - designHeight * scale) / 2
    if policy == 'fill' or policy == 'expand' or policy == 'fitWidth' then
        return scale, scale, offsetX, offsetY, width / scale, height / scale
    end
    return scale, scale, offsetX, offsetY, designWidth, designHeight
end

-- Sets the rows of a table of two columns from pairs of a name and a value, only when a value changed.
function Scaling:show(id, pairs)
    local key = table.concat(pairs, '|')
    if self[id] == key then
        return
    end
    self[id] = key
    local rows = {}
    for index = 1, #pairs, 2 do
        rows[#rows + 1] = {id = id .. index, cells = {pairs[index], pairs[index + 1]}}
    end
    self:set(id, {rows = rows})
end

-- Formats a number with the decimals it needs, never as "-0".
function Scaling.number(value, decimals)
    local text = string.format('%.' .. (decimals or 0) .. 'f', value)
    return (text:gsub('^%-(0[%.0]*)$', '%1'))
end

function Scaling:update(dt)
    Scaling.super.update(self, dt)
    local screen = self.screen
    local scaleX, scaleY, offsetX, offsetY, visibleWidth, visibleHeight = self:map(screen.width, screen.height)
    local designWidth, designHeight = viewport.designSize()
    local n = Scaling.number
    self:show('mapping', {
        'Scale', scaleX == scaleY and n(scaleX, 2) or n(scaleX, 2) .. ' by ' .. n(scaleY, 2),
        'Design', string.format('%s by %s pixels', n(designWidth * scaleX), n(designHeight * scaleY)),
        'Placed at', string.format('%s, %s', n(offsetX), n(offsetY)),
        'Visible', string.format('%s by %s units', n(visibleWidth), n(visibleHeight)),
    })
    local visible = viewport.visibleRect()
    local unitX = viewport.pixelsPerUnit()
    local width, height = window.framebufferSize()
    self:show('live', {
        'Design', string.format('%s by %s, "%s"', n(designWidth), n(designHeight), viewport.scaling()),
        'Window', string.format('%s by %s pixels', n(width), n(height)),
        'Visible', string.format('%s by %s units at %s, %s', n(visible.width), n(visible.height), n(visible.x), n(visible.y)),
        'Per unit', string.format('%s pixels', n(unitX, 2)),
    })
    self:status(string.format('The policy "%s" maps the design onto the %s.', self.policy, self.screen.text:lower()))
end

-- Draws the design area through `place`, which maps design units to the preview: the painted scenery of the app, a HUD in its top-left corner and coins in its other corners, which show where the policy puts the edges of the design.
function Scaling:drawDesign(place, scaleX, scaleY)
    local designWidth, designHeight = viewport.designSize()
    local left, top = place(0, 0)
    graphics2d.draw(self.scenery, left, top, {width = designWidth * scaleX, height = designHeight * scaleY, pivotX = 0, pivotY = 0, layer = 1})
    local hudX, hudY = place(40, 40)
    graphics2d.drawShape({hudX, hudY, 380 * scaleX, 120 * scaleY}, {radius = 30 * math.min(scaleX, scaleY), color = '#C0151826', borderWidth = 6 * math.min(scaleX, scaleY), borderColor = '#FF15172A', layer = 2})
    for index = 0, 2 do
        local x, y = place(70 + index * 105, 60)
        graphics2d.draw(self.heart, x, y, {width = 80 * scaleX, height = 80 * scaleY, pivotX = 0, pivotY = 0, layer = 3})
    end
    for _, corner in ipairs({{designWidth - 130, 40}, {40, designHeight - 130}, {designWidth - 130, designHeight - 130}}) do
        local x, y = place(corner[1], corner[2])
        graphics2d.draw(self.coin, x, y, {width = 90 * scaleX, height = 90 * scaleY, pivotX = 0, pivotY = 0, layer = 3})
    end
    local right, bottom = place(designWidth, designHeight)
    graphics2d.drawShape({left, top, right - left, bottom - top}, {color = '#00000000', borderWidth = 3, borderColor = '#E6FFFFFF', layer = 4})
end

-- The simulated screen sits in the device it belongs to, a phone, a tablet or a monitor, and shows the extra area of "expand" as a soft hatched band, the letterboxing as black and the design area in its place.
function Scaling:draw(area)
    local screen = self.screen
    local fit = math.min((area.width - 160) / screen.width, (area.height - 140) / screen.height)
    local screenRect = {(area.width - screen.width * fit) / 2, (area.height - screen.height * fit) / 2, screen.width * fit, screen.height * fit}
    local scaleX, scaleY, offsetX, offsetY = self:map(screen.width, screen.height)
    local device, radius = Device.around(screen.device, screenRect, screen.sideways)
    device.screen = '#FF000000'
    device.layer = -10
    Device.draw(device)

    graphics2d.pushMask(function()
        graphics2d.drawShape(screenRect, {radius = radius, color = '#FFFFFFFF'})
    end)
    -- The policy `expand` shows more than the design area, which the app is expected to fill.
    if self.policy == 'expand' then
        graphics2d.drawRect(screenRect, '#FF262B3D')
        for offset = -screenRect[4], screenRect[3], 28 do
            graphics2d.drawLine(screenRect[1] + offset, screenRect[2] + screenRect[4], screenRect[1] + offset + screenRect[4], screenRect[2], 8, '#1AFFFFFF')
        end
    end
    self:drawDesign(function(x, y)
        return screenRect[1] + (offsetX + x * scaleX) * fit, screenRect[2] + (offsetY + y * scaleY) * fit
    end, scaleX * fit, scaleY * fit)
    graphics2d.popMask()
end

return Scaling
