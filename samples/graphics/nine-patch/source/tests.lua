-- The tests of the sample in menu order. Each module returns the scene class of its test.
return {
    {id = 'classic', title = 'Stretch and tile', description = 'One framed image cut by borders, with its edges and center stretched or repeated.', module = 'tests.classic'},
    {id = 'pieces', title = 'Nine pieces', description = 'A frame made of nine separate regions of a sheet instead of borders.', module = 'tests.pieces'},
    {id = 'scale-tint', title = 'Scale and tint', description = 'The same frame with its borders scaled and its colors tinted.', module = 'tests.scale-tint'},
    {id = 'ui-theme', title = 'UI theme surfaces', description = 'Panels, buttons, sliders and progress bars drawn with nine-slice theme surfaces, colorized by each component.', module = 'tests.ui-theme'},
    {id = 'resizable', title = 'Resizable panel', description = 'A panel dragged and resized by its edges and corners with the mouse or a finger.', module = 'tests.resizable'},
}
