-- The scale of the interface: design units that follow the screen, or a physical size that the density of the screen decides, times a factor, with the size of a control in points on this screen.
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local UiScale = haylen.class('UiScale', Test)

UiScale.modes = {{id = 'design', text = 'Design'}, {id = 'physical', text = 'Physical'}}

function UiScale:enter()
    self.started = {mode = ui.scaleMode(), factor = ui.scale()}
    self:frame{
        hint = 'Pick the scale mode and the factor, then resize the window or run the test on a phone and a tablet. In the physical mode the sample button keeps the same size in points on every screen, and the frame of the test reflows into the room it has.',
        focus = 'mode',
        content = {ui.scroll{grow = 1, height = 0, layout.columns{
            ui.column{grow = 1, gap = 24,
                layout.section('Scale', {
                    ui.segmentedControl{id = 'mode', items = UiScale.modes, selected = ui.scaleMode(), onChange = function(event) ui.setScaleMode(event.value) end},
                    ui.slider{id = 'factor', min = 0.5, max = 2, step = 0.25, value = self.started.factor, showValue = true, onChange = function(event) ui.setScale(event.value) end},
                }),
                layout.section('A control of 64 units', {ui.button{id = 'sample', text = 'Sample button', variant = 'primary'}}),
            },
            ui.column{grow = 1, gap = 24,
                layout.section('Text in every role', {
                    ui.label{text = 'A title', font = 'title'},
                    ui.label{text = 'A heading', font = 'heading'},
                    ui.label{text = 'Body text that wraps when the room gets small, as it does on a phone in the physical mode.', wrap = true},
                    ui.label{text = 'A caption', font = 'caption', color = 'textMuted'},
                }),
            },
        }}},
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
    self:status(string.format('Mode "%s", factor %.2f, %.2f design units per UI unit, %.2f pixels per point. The sample button is %.0f points tall.', ui.scaleMode(), factor, scale, window.dpiScale(), points))
end

return UiScale
