-- The tests of the category in menu order.
return {
    prefix = 'NSL',
    title = 'Nine-slice',
    description = 'Frames cut into nine regions that stretch or tile, from borders or pieces, scaled, tinted, in UI themes and resized by hand.',
    tests = {
        {code = 'NSL-001', title = 'Stretch and tile', description = 'One framed image cut by borders, with its edges and center stretched or repeated.', module = 'stretch-tile'},
        {code = 'NSL-002', title = 'Nine pieces', description = 'A frame made of nine separate regions of a sheet instead of borders.', module = 'pieces'},
        {code = 'NSL-003', title = 'Scale and tint', description = 'The same frame with its borders scaled and its colors tinted.', module = 'scale-tint'},
        {code = 'NSL-004', title = 'UI theme surfaces', description = 'Panels, buttons, sliders and progress bars drawn with nine-slice theme surfaces, colorized by each component.', module = 'theme-surfaces'},
        {code = 'NSL-005', title = 'Resizable panel', description = 'A panel dragged and resized by its edges and corners with the mouse, a finger or a gamepad.', module = 'resizable'},
        {code = 'NSL-006', title = 'Slices of trimmed atlas frames', description = 'A nine-slice of an atlas on a frame the packer trimmed, which must read the same pixels as the frame.', module = 'trimmed-slices'},
    },
}
