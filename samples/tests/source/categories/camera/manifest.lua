-- The tests of the category in menu order.
return {
    prefix = 'CAM',
    title = 'Camera',
    description = 'The 2D camera following, framing, shaking, turning and splitting the view, with parallax, pixel snap, picking and debug drawing.',
    tests = {
        {code = 'CAM-001', title = 'Dead zone and drag margins', description = 'Following a walker that moves freely inside a dead zone or inside drag margins.', module = 'follow'},
        {code = 'CAM-002', title = 'Smoothing and look-ahead', description = 'Position smoothing and a view that leads the walker by its velocity.', module = 'smoothing'},
        {code = 'CAM-003', title = 'Limits', description = 'A view that stays inside limits, stopping at them or easing into them.', module = 'limits'},
        {code = 'CAM-004', title = 'Zoom', description = 'Zoom around the pointer with the wheel, a pinch, keys or triggers, within limits.', module = 'zoom'},
        {code = 'CAM-005', title = 'Framing several targets', description = 'A camera that keeps three walkers in view, zooming out as they spread.', module = 'framing'},
        {code = 'CAM-006', title = 'Shake', description = 'Trauma shake in every direction and directional recoil.', module = 'shake'},
        {code = 'CAM-007', title = 'Rotation', description = 'A turning view with rotation smoothing, and "ignoreRotation" keeping it upright.', module = 'rotation'},
        {code = 'CAM-008', title = 'Split screen', description = 'Two walkers, each with a camera in its own half of the play area.', module = 'split-screen'},
        {code = 'CAM-009', title = 'Minimap', description = 'A second camera in a corner viewport that leaves details out with visibility bits.', module = 'minimap'},
        {code = 'CAM-010', title = 'Blending cameras', description = 'Smooth cuts between a walker camera and a landmark camera.', module = 'blend'},
        {code = 'CAM-011', title = 'Parallax layers', description = 'Mountains, hills, clouds and grass scrolling at their own rates.', module = 'parallax'},
        {code = 'CAM-012', title = 'Pixel snap', description = 'Pixel art with and without snapping the view to whole pixels.', module = 'pixel-snap'},
        {code = 'CAM-013', title = 'Screen to world', description = 'Picking objects under the pointer in a zoomed and rotated view.', module = 'picking'},
        {code = 'CAM-014', title = 'Debug drawing', description = 'The view, the limits and the drag box drawn from a zoomed-out camera.', module = 'debug'},
    },
}
