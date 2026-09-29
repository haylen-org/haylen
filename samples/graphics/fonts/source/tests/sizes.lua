-- TrueType and OpenType sizes: both kinds render from a signed distance field baked once, so every size stays sharp, and a zoomed word shows it from 8 to 320 units.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local fonts = require('fonts')
local sample = require('sample')

local Sizes = haylen.class('Sizes', sample.Test)

Sizes.hints = 'Drag the zoom slider, or focus it and press left and right. With pulse on, the word breathes between 8 and 320 units, and no size needs an atlas of its own.'
Sizes.focus = 'zoom'

local kSizes = {12, 16, 20, 24, 32, 48, 64, 96}
local kLine = 'Black sphinx'

function Sizes:init(entry)
    Sizes.super.init(self, entry)
    self.zoom = 120
    self.pulse = true
end

function Sizes:controls()
    return {
        ui.label{text = 'Zoom'},
        ui.slider{id = 'zoom', min = 8, max = 320, value = self.zoom, step = 1, showValue = true, decimals = 0, onChange = function(event)
            self.zoom = event.value
            self.pulse = false
            event.document:set('pulse', {checked = false})
        end},
        ui.toggle{id = 'pulse', text = 'Pulse', checked = self.pulse, onChange = function(event) self.pulse = event.checked end},
    }
end

function Sizes:update(dt)
    if self.pulse then
        self.zoom = 164 + 156 * math.sin(haylen.elapsed() * 0.8)
    end
    local ttf, otf = fonts.get('crimson'), fonts.get('fira')
    self:setStatus(string.format('TrueType bake size %d, %d atlas pages. OpenType bake size %d, %d atlas pages. Zoom %.0f.', ttf.nativeSize, ttf.pageCount, otf.nativeSize, otf.pageCount, self.zoom))
end

-- One column of lines in a font, each after its size.
local function column(font, title, x, y)
    sample.caption(title, x, y, {size = 26, color = '#FF8FB0FF'})
    y = y + 44
    for _, size in ipairs(kSizes) do
        sample.caption(tostring(size), x, y + font:ascent(size) - 18)
        graphics2d.drawText(font, kLine, x + 56, y, {size = size})
        y = y + font:lineHeight(size) + 6
    end
    return y
end

function Sizes:render()
    local stage = self:stage()
    if stage == nil then
        return
    end
    graphics2d.beginScreen()
    graphics2d.pushClip(stage)
    local half = stage.width / 2
    local bottom = column(fonts.get('crimson'), 'Crimson Text, TrueType (.ttf)', stage.x, stage.y)
    column(fonts.get('fira'), 'Fira Sans, OpenType with CFF outlines (.otf)', stage.x + half, stage.y)
    local y = bottom + 20
    graphics2d.drawLine(stage.x, y, stage:right(), y, 2, sample.guide)
    graphics2d.drawText(fonts.get('crimson'), 'Zoom', stage.x + half / 2, y + 20, {size = self.zoom, anchor = {0.5, 0}, color = '#FFFFE070'})
    graphics2d.drawText(fonts.get('fira'), 'Zoom', stage.x + half * 1.5, y + 20, {size = self.zoom, anchor = {0.5, 0}, color = '#FF7FCBF2'})
    graphics2d.popClip()
end

return Sizes
