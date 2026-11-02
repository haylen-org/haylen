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
-- The factors the player picks, named where a name says more than the number.
UiScale.factors = {{id = '0.5', text = 'Half'}, {id = '0.75', text = '0.75'}, {id = '1', text = 'Normal'}, {id = '1.25', text = '1.25'}, {id = '1.5', text = '1.5'}, {id = '2', text = 'Twice'}}

-- Returns the id of the factor nearest to `value`.
function UiScale.factorId(value)
    local best, distance = UiScale.factors[1].id, math.huge
    for _, factor in ipairs(UiScale.factors) do
        local gap = math.abs(tonumber(factor.id) - value)
        if gap < distance then
            best, distance = factor.id, gap
        end
    end
    return best
end

function UiScale:enter()
    self.started = {mode = ui.scaleMode(), factor = ui.scale()}
    self:frame{
        hint = 'Pick the scale mode and the factor, then resize the window or run the test on a phone and a tablet. In the physical mode the sample button keeps the same size in points on every screen, and the frame of the test reflows into the room it has.',
        code = UiScale.code,
        focus = 'mode',
        content = {layout.columns{
            ui.column{grow = 1, gap = 24, align = 'stretch',
                layout.section('Scale', {
                    ui.formField{label = 'Mode', ui.segmentedControl{id = 'mode', items = UiScale.modes, selected = ui.scaleMode(), onChange = function(event) ui.setScaleMode(event.value) end}},
                    ui.formField{label = 'Factor', ui.segmentedControl{id = 'factor', items = UiScale.factors, selected = UiScale.factorId(self.started.factor), onChange = function(event) ui.setScale(tonumber(event.value)) end}},
                }),
                layout.section('How tall a control is', {grow = 1,
                    ui.row{gap = 24, alignItems = 'center',
                        ui.button{id = 'sample', text = 'Sample button', variant = 'primary'},
                        ui.divider{vertical = true, height = 64, color = 'accent'},
                        ui.column{gap = 4, grow = 1, ui.label{id = 'measure', text = '', font = 'heading', color = 'accentText'}, ui.label{id = 'unit', text = '', color = 'textMuted'}},
                    },
                }),
            },
            ui.column{grow = 1, gap = 24, align = 'stretch',
                layout.section('Text in every role', {
                    ui.label{text = 'A title', font = 'title'},
                    ui.label{text = 'A heading', font = 'heading'},
                    ui.label{text = 'Body text that wraps when the room gets small, as it does on a phone in the physical mode.', wrap = true},
                    ui.label{text = 'A caption', font = 'caption', color = 'textMuted'},
                }),
                layout.section('This screen', {grow = 1, ui.table{id = 'screen', columns = {{text = 'Measure'}, {text = 'Value', width = 260, align = 'end'}}}}),
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
    local pointsPerUnit = pixelsPerUnit / window.dpiScale()
    local units = bounds.height / scale
    local key = string.format('%.2f %.2f %.2f %.2f', factor, scale, pointsPerUnit, window.dpiScale())
    if key ~= self.shown then
        self.shown = key
        self:set('measure', {text = string.format('%.0f units, %.0f points tall', units, bounds.height * pointsPerUnit)})
        self:set('unit', {text = string.format('One UI unit spans %.2f points on this screen.', scale * pointsPerUnit)})
        self:set('screen', {rows = {
            {id = 'mode', cells = {'Scale mode', ui.scaleMode() == 'design' and 'Design' or 'Physical'}},
            {id = 'factor', cells = {'Factor', string.format('%.2f', factor)}},
            {id = 'design', cells = {'Design units for each UI unit', string.format('%.2f', scale)}},
            {id = 'density', cells = {'Pixels for each point', string.format('%.2f', window.dpiScale())}},
        }})
    end
    self:status(string.format('The %s mode at a factor of %.2f draws %.2f design units for each UI unit, on a screen of %.2f pixels per point.', ui.scaleMode(), factor, scale, window.dpiScale()))
end

return UiScale
