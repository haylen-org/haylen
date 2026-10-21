-- The scale of the interface: design units that follow the screen, or a physical size that the density of the screen decides, times a factor, with the size of a control in points on this screen.
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local UiScale = haylen.class('UiScale', Test)

UiScale.code = [[
ui.setScaleMode('physical')  -- A UI unit keeps three quarters of a point on every screen.
ui.setScale(1.25)
local factor, scale, points = ui.scale()
local tall = gui:bounds('sample').height / scale * points  -- The height of the button in points.]]

UiScale.modes = {{id = 'design', text = 'Design'}, {id = 'physical', text = 'Physical'}}

function UiScale:enter()
    self.started = {mode = ui.scaleMode(), factor = ui.scale()}
    self:frame{
        hint = 'Pick the scale mode and the factor, then resize the window or run the test on a phone and a tablet. In the physical mode the sample button keeps the same size in points on every screen, and the frame of the test reflows into the room it has.',
        code = UiScale.code,
        focus = 'mode',
        content = {layout.columns{
            ui.column{grow = 1, gap = 24,
                layout.section('Scale', {
                    ui.segmentedControl{id = 'mode', items = UiScale.modes, selected = ui.scaleMode(), onChange = function(event) ui.setScaleMode(event.value) end},
                    ui.formField{label = 'Scale factor, from half to twice the size', ui.slider{id = 'factor', min = 0.5, max = 2, step = 0.25, value = self.started.factor, showValue = true, onChange = function(event) ui.setScale(event.value) end}},
                }),
                layout.section('A control of 64 units', {
                    ui.row{gap = 16, ui.button{id = 'sample', text = 'Sample button', variant = 'primary'}, ui.divider{vertical = true, height = 64, color = 'accent'}, ui.label{id = 'measure', text = '', color = 'accentText'}},
                }),
            },
            ui.column{grow = 1, gap = 24,
                layout.section('Text in every role', {
                    ui.label{text = 'A title', font = 'title'},
                    ui.label{text = 'A heading', font = 'heading'},
                    ui.label{text = 'Body text that wraps when the room gets small, as it does on a phone in the physical mode.', wrap = true},
                    ui.label{text = 'A caption', font = 'caption', color = 'textMuted'},
                }),
            },
        }},
    }
end

function UiScale:exit()
    ui.setScaleMode(self.started.mode)
    ui.setScale(self.started.factor)
    UiScale.super.exit(self)
end

function UiScale:update(dt)
    UiScale.super.update(self, dt)
    local bounds = self.gui:bounds('sample')
    if not bounds then
        return
    end
    local factor, scale = ui.scale()
    local _, pixelsPerUnit = viewport.pixelsPerUnit()
    local points = bounds.height * pixelsPerUnit / window.dpiScale()
    self:set('measure', {text = string.format('%.0f points tall', points)})
    self:status(string.format('The %s mode at a factor of %.2f draws %.2f design units for each UI unit, on a screen of %.2f pixels per point.', ui.scaleMode(), factor, scale, window.dpiScale()))
end

return UiScale
