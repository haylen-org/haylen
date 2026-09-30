-- The tests of the sample in menu order. Each module returns a scene class that takes its entry.
return {
    {id = 'follow', title = 'Dead zone and drag margins', description = 'Following a player that moves freely inside a dead zone or inside drag margins.', module = 'tests.follow'},
    {id = 'smoothing', title = 'Smoothing and look-ahead', description = 'Position smoothing and a view that leads the player by its velocity.', module = 'tests.smoothing'},
    {id = 'limits', title = 'Limits', description = 'A view that stays inside limits, stopping at them or easing into them.', module = 'tests.limits'},
    {id = 'zoom', title = 'Zoom', description = 'Zoom around the pointer with the wheel, a pinch, keys or triggers, within limits.', module = 'tests.zoom'},
    {id = 'framing', title = 'Framing several targets', description = 'A camera that keeps three players in view, zooming out as they spread.', module = 'tests.framing'},
    {id = 'shake', title = 'Shake', description = 'Trauma shake in every direction and directional recoil.', module = 'tests.shake'},
    {id = 'rotation', title = 'Rotation', description = 'A turning view with rotation smoothing, and "ignoreRotation" keeping it upright.', module = 'tests.rotation'},
    {id = 'split-screen', title = 'Split screen', description = 'Two players, each with a camera in its own half of the screen.', module = 'tests.split-screen'},
    {id = 'minimap', title = 'Minimap', description = 'A second camera in a corner viewport that leaves details out with visibility bits.', module = 'tests.minimap'},
    {id = 'blend', title = 'Blending cameras', description = 'Smooth cuts between a player camera and a landmark camera.', module = 'tests.blend'},
    {id = 'parallax', title = 'Parallax layers', description = 'Mountains, hills, clouds and grass scrolling at their own rates.', module = 'tests.parallax'},
    {id = 'pixel-snap', title = 'Pixel snap', description = 'Pixel art with and without snapping the view to whole pixels.', module = 'tests.pixel-snap'},
    {id = 'picking', title = 'Screen to world', description = 'Picking objects under the pointer in a zoomed and rotated view.', module = 'tests.picking'},
    {id = 'debug', title = 'Debug drawing', description = 'The view, the limits and the drag box drawn from a zoomed-out camera.', module = 'tests.debug'},
}
