-- Design resolution and scaling: the app lays out in design units, and the scaling policy of app.json maps them onto the screen. The preview applies each policy to screens of other shapes, and the live values show how this app is mapped right now.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local sample = require('sample')

local Scaling = haylen.class('Scaling', sample.Test)

Scaling.hints = 'Left and right change the focused stepper. The policy is design.scaling in app.json, and this app uses expand. The preview draws the design area of 1920 by 1080 on a simulated screen: black bars are letterboxing, the striped band is the extra visible area of expand, and whatever leaves the screen is cropped.'
Scaling.focus = 'policy'

Scaling.policies = {{id = 'fit', text = 'fit'}, {id = 'fill', text = 'fill'}, {id = 'stretch', text = 'stretch'}, {id = 'expand', text = 'expand'}, {id = 'pixel_perfect', text = 'pixel perfect'}}
Scaling.screens = {
    {id = 'phone-portrait', text = 'Phone in portrait', width = 1170, height = 2532},
    {id = 'phone-landscape', text = 'Phone in landscape', width = 2532, height = 1170},
    {id = 'tablet', text = 'Tablet', width = 2048, height = 1536},
    {id = 'ultrawide', text = 'Ultrawide monitor', width = 3440, height = 1440},
    {id = 'full-hd', text = 'Full HD monitor', width = 1920, height = 1080},
    {id = 'small', text = 'Small window', width = 800, height = 600},
}

function Scaling:init(entry)
    Scaling.super.init(self, entry)
    self.policy = viewport.scaling()
    self.screen = Scaling.screens[1]
end

function Scaling:content()
    local screens = {}
    for index, screen in ipairs(Scaling.screens) do
        screens[index] = {id = screen.id, text = string.format('%s, %d x %d', screen.text, screen.width, screen.height)}
    end
    return ui.row{gap = 24,
        ui.column{width = 760, gap = 24, align = 'start',
            sample.section('preview', {
                ui.stepper{id = 'policy', items = Scaling.policies, selected = self.policy, wrap = true, onChange = function(event) self.policy = event.value end},
                ui.stepper{id = 'screen', items = screens, selected = self.screen.id, wrap = true, onChange = function(event)
                    for _, screen in ipairs(Scaling.screens) do
                        if screen.id == event.value then
                            self.screen = screen
                        end
                    end
                end},
                ui.label{id = 'mapping', text = '', font = 'monospace'},
            }),
            sample.section('this app now', {ui.label{id = 'live', text = '', font = 'monospace'}}),
        },
        ui.spacer{id = 'preview', grow = 1, align = 'stretch'},
    }
end

-- Maps the design area onto a screen of `width` by `height` pixels the way the engine does, and returns the scale on each axis, the pixel offset of the design origin and the visible design width and height.
function Scaling:map(width, height)
    local designWidth, designHeight = viewport.designSize()
    local ratioX, ratioY = width / designWidth, height / designHeight
    local policy = self.policy
    if policy == 'stretch' then
        return ratioX, ratioY, 0, 0, designWidth, designHeight
    end
    local scale = math.min(ratioX, ratioY)
    if policy == 'fill' then
        scale = math.max(ratioX, ratioY)
    elseif policy == 'pixel_perfect' then
        scale = math.max(1, math.floor(scale))
    end
    local offsetX, offsetY = (width - designWidth * scale) / 2, (height - designHeight * scale) / 2
    if policy == 'fill' or policy == 'expand' then
        return scale, scale, offsetX, offsetY, width / scale, height / scale
    end
    return scale, scale, offsetX, offsetY, designWidth, designHeight
end

function Scaling:update(dt)
    local screen = self.screen
    local scaleX, scaleY, offsetX, offsetY, visibleWidth, visibleHeight = self:map(screen.width, screen.height)
    local designWidth, designHeight = viewport.designSize()
    self:show('mapping', string.format('scale    %.3f x %.3f\ndesign   %.0f x %.0f pixels at %.0f, %.0f\nvisible  %.0f x %.0f design units', scaleX, scaleY, designWidth * scaleX, designHeight * scaleY, offsetX, offsetY, visibleWidth, visibleHeight))
    local visible, pixels = viewport.visibleRect(), viewport.pixelRect()
    local unitX, unitY = viewport.pixelsPerUnit()
    local width, height = window.size()
    self:show('live', string.format('design   %.0f x %.0f, %s\nwindow   %.0f x %.0f pixels\nvisible  %.0f, %.0f, %.0f x %.0f units\npixels   %.0f, %.0f, %.0f x %.0f\nper unit %.3f x %.3f pixels', designWidth, designHeight, viewport.scaling(), width, height, visible.x, visible.y, visible.width, visible.height, pixels.x, pixels.y, pixels.width, pixels.height, unitX, unitY))
    self:setStatus(string.format('%s on the %s', self.policy, self.screen.text:lower()))
end

-- Draws a small picture of the design area through `place`, which maps design units to the preview.
local function drawDesign(place, scaleX, scaleY)
    local designWidth, designHeight = viewport.designSize()
    local function corner(x, y)
        local px, py = place(x, y)
        return {x = px, y = py}
    end
    local function rect(x, y, width, height, color)
        local left, top = place(x, y)
        graphics2d.drawRect({left, top, width * scaleX, height * scaleY}, color)
    end
    local function ellipse(cx, cy, radius, color)
        local points = {}
        for step = 0, 23 do
            local angle = step / 24 * math.pi * 2
            points[#points + 1] = {place(cx + math.cos(angle) * radius, cy + math.sin(angle) * radius)}
        end
        graphics2d.drawPolygon(points, color)
    end
    local sky = {corner(0, 0), corner(designWidth, 0), corner(designWidth, 800), corner(0, 800)}
    sky[1].color, sky[2].color, sky[3].color, sky[4].color = '#FF4FA3E0', '#FF4FA3E0', '#FFBFE6FF', '#FFBFE6FF'
    graphics2d.drawMesh(nil, sky, {1, 2, 3, 1, 3, 4})
    ellipse(1500, 260, 120, '#FFFFF3B0')
    graphics2d.drawPolygon({{place(0, 800)}, {place(400, 520)}, {place(760, 800)}}, '#FF6D8FB3')
    graphics2d.drawPolygon({{place(560, 800)}, {place(1000, 460)}, {place(1440, 800)}}, '#FF5B7AA0')
    rect(0, 800, designWidth, designHeight - 800, '#FF3F7A57')
    rect(1180, 600, 300, 200, '#FFF3E6C4')
    graphics2d.drawPolygon({{place(1150, 600)}, {place(1330, 460)}, {place(1510, 600)}}, '#FFB53A2E')
    rect(40, 40, 560, 110, '#C0101418')
    local size = math.min(scaleX, scaleY)
    local hudX, hudY = place(70, 60)
    graphics2d.drawText(nil, 'HUD at 40, 40', hudX, hudY, {size = 56 * size})
    local endX, endY = place(designWidth - 30, designHeight - 30)
    graphics2d.drawText(nil, '1920, 1080', endX, endY, {size = 48 * size, anchor = {1, 1}})
    graphics2d.drawPolyline({{place(0, 0)}, {place(designWidth, 0)}, {place(designWidth, designHeight)}, {place(0, designHeight)}}, 3, '#FFFFFFFF', true)
end

function Scaling:render()
    local area = self.document:bounds('preview')
    if area == nil then
        return
    end
    local screen = self.screen
    local fit = math.min((area.width - 60) / screen.width, (area.height - 60) / screen.height)
    local screenRect = {area.x + (area.width - screen.width * fit) / 2, area.y + (area.height - screen.height * fit) / 2, screen.width * fit, screen.height * fit}
    local scaleX, scaleY, offsetX, offsetY = self:map(screen.width, screen.height)

    graphics2d.beginScreen()
    graphics2d.drawRect({screenRect[1] - 18, screenRect[2] - 18, screenRect[3] + 36, screenRect[4] + 36}, '#FF2C3147')
    graphics2d.drawRect(screenRect, '#FF000000')
    graphics2d.pushClip(screenRect)
    -- Expand shows more than the design area, which the app is expected to fill.
    if self.policy == 'expand' then
        for offset = -screenRect[4], screenRect[3], 24 do
            graphics2d.drawLine(screenRect[1] + offset, screenRect[2] + screenRect[4], screenRect[1] + offset + screenRect[4], screenRect[2], 6, '#FF3A4058')
        end
    end
    drawDesign(function(x, y)
        return screenRect[1] + (offsetX + x * scaleX) * fit, screenRect[2] + (offsetY + y * scaleY) * fit
    end, scaleX * fit, scaleY * fit)
    graphics2d.popClip()
end

return Scaling
