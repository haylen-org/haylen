-- The tests of the category in menu order.
return {
    prefix = 'LIT',
    title = 'Lighting',
    description = 'Lit canvases with ambient, point, spot and directional lights, shadows, normal maps, emission, masks and many lights at once.',
    tests = {
        {code = 'LIT-001', title = 'Ambient light', description = 'The color a lit canvas starts from before any light, from noon to a dark cave.', module = 'ambient'},
        {code = 'LIT-002', title = 'Point lights', description = 'Colored point lights with their own radius and intensity, one on the cursor and more where you place them.', module = 'point-lights'},
        {code = 'LIT-003', title = 'Spot lights', description = 'Cones that aim at the cursor and sweep the room, with their inner and outer angles.', module = 'spot-lights'},
        {code = 'LIT-004', title = 'Directional light', description = 'A sun that covers the whole canvas from one direction and casts long shadows.', module = 'directional'},
        {code = 'LIT-005', title = 'Blend modes', description = 'Lights that add, subtract or mix into the light map.', module = 'blend-modes'},
        {code = 'LIT-006', title = 'Intensity above 1', description = 'Lights brighter than the unlit colors in a floating-point light map.', module = 'hdr'},
        {code = 'LIT-007', title = 'Shadows', description = 'Occluders, the "none", "pcf5" and "pcf13" filters, shadow color and smoothness.', module = 'shadows'},
        {code = 'LIT-008', title = 'Physics and Tiled occluders', description = 'Shadows cast by falling physics crates and by the walls of a Tiled object layer.', module = 'occluders'},
        {code = 'LIT-009', title = 'Normal maps and specular', description = 'Bricks and studs with normal maps generated in Lua, lit by the angle of the light with highlights.', module = 'normal-maps'},
        {code = 'LIT-010', title = 'Unshaded and emissive', description = 'Signs that keep their colors in the dark and windows and neon that glow.', module = 'emission'},
        {code = 'LIT-011', title = 'Light masks and layers', description = 'Lights that reach only the draws whose mask and layer they select.', module = 'masks'},
        {code = 'LIT-012', title = 'Flicker', description = 'Torches that waver like flames, each with its own seed.', module = 'flicker'},
        {code = 'LIT-013', title = 'Day and night', description = 'A day cycle written in Lua: ambient color, sun, street lamps and lit windows.', module = 'day-night'},
        {code = 'LIT-014', title = 'Lit render targets', description = 'Lit canvases drawn into render targets and shown as screens.', module = 'render-targets'},
        {code = 'LIT-015', title = 'Many lights', description = 'Hundreds of moving lights with the renderer counters.', module = 'stress'},
    },
}
