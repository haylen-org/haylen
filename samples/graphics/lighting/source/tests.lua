-- The tests of the sample in menu order. Each module returns a scene class that takes its entry.
return {
    {id = 'ambient', title = 'Ambient light', description = 'The color a lit canvas starts from before any light, from noon to a dark cave.', module = 'tests.ambient'},
    {id = 'point-lights', title = 'Point lights', description = 'Colored point lights with their own radius and intensity, one on the cursor and more where you place them.', module = 'tests.point-lights'},
    {id = 'spot-lights', title = 'Spot lights', description = 'Cones that aim at the cursor and sweep the room, with their inner and outer angles.', module = 'tests.spot-lights'},
    {id = 'directional', title = 'Directional light', description = 'A sun that covers the whole canvas from one direction and casts long shadows.', module = 'tests.directional'},
    {id = 'blend-modes', title = 'Blend modes', description = 'Lights that add, subtract or mix into the light map.', module = 'tests.blend-modes'},
    {id = 'hdr', title = 'Intensity above 1', description = 'Lights brighter than the unlit colors in a floating-point light map.', module = 'tests.hdr'},
    {id = 'shadows', title = 'Shadows', description = 'Occluders, the none, PCF5 and PCF13 filters, shadow color and smoothness.', module = 'tests.shadows'},
    {id = 'occluders', title = 'Physics and Tiled occluders', description = 'Shadows cast by falling physics crates and by the walls of a Tiled object layer.', module = 'tests.occluders'},
    {id = 'normal-maps', title = 'Normal maps and specular', description = 'Bricks and studs with normal maps generated in Lua, lit by the angle of the light with highlights.', module = 'tests.normal-maps'},
    {id = 'emission', title = 'Unshaded and emissive', description = 'Signs that keep their colors in the dark and windows and neon that glow.', module = 'tests.emission'},
    {id = 'masks', title = 'Light masks and layers', description = 'Lights that reach only the draws whose mask and layer they select.', module = 'tests.masks'},
    {id = 'flicker', title = 'Flicker', description = 'Torches that waver like flames, each with its own seed.', module = 'tests.flicker'},
    {id = 'day-night', title = 'Day and night', description = 'A day cycle written in Lua: ambient color, sun, street lamps and lit windows.', module = 'tests.day-night'},
    {id = 'render-targets', title = 'Lit render targets', description = 'Lit canvases drawn into render targets and shown as screens.', module = 'tests.render-targets'},
    {id = 'stress', title = 'Many lights', description = 'Hundreds of moving lights with the renderer counters.', module = 'tests.stress'},
}
