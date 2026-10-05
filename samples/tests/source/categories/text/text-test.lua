-- The base of the text tests whose lines keep fixed places: the frame fits a layout of a fixed size into the play area, with 0, 0 at the top-left corner of the layout, so every screen shows the same page at its own scale.
local haylen = require('haylen')
local m = require('haylen.math')

local Test = require('harness.test')

local TextTest = haylen.class('TextTest', Test)

TextTest.wide = {1860, 840}
TextTest.narrow = {1380, 840}

-- Takes the options of `Test:frame`, where `view` defaults to the narrow layout next to a panel of controls and to the wide one without it.
function TextTest:frame(options)
    options.view = options.view or (options.controls and TextTest.narrow or TextTest.wide)
    TextTest.super.frame(self, options)
    self.layout = m.rect(0, 0, options.view[1], options.view[2])
    self.camera.position = {options.view[1] / 2, options.view[2] / 2}
end

return TextTest
