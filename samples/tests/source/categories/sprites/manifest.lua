-- The tests of the category in menu order.
return {
    prefix = 'SPR',
    title = 'Sprites',
    description = 'Sprites, atlases, animation, batches, pools, render targets, shapes and text of the 2D renderer.',
    tests = {
        {code = 'SPR-001', title = 'Sprite basics', description = 'Pivot, rotation, scale, flips, tint and flash on one sprite.', module = 'basics'},
        {code = 'SPR-002', title = 'Atlases and sheets', description = 'A hash atlas with trimmed frames, an array atlas with tags and a slice, and a grid sheet.', module = 'atlases'},
        {code = 'SPR-003', title = 'Animation', description = 'Clips, loop modes, frame and finish events and a queue of clips on an animator.', module = 'animation'},
        {code = 'SPR-004', title = 'Sprite batches', description = 'Many sprites of one texture kept in C++ and drawn in one batch, added, changed and removed.', module = 'batches'},
        {code = 'SPR-005', title = 'Static batches', description = 'A tile map baked once to the GPU and drawn every frame without uploads, with parallax offsets.', module = 'static-batches'},
        {code = 'SPR-006', title = 'Layers and y-sort', description = 'Heroes among trees sorted by the y they stand on, and a selected hero raised above every layer.', module = 'layers'},
        {code = 'SPR-007', title = 'Pooled bullets', description = 'A bullet hell whose projectiles come from an object pool and draw in one batch.', module = 'bullets'},
        {code = 'SPR-008', title = 'Bouncing bunnies', description = 'From a few to many thousands of sprites with float buffers or one table each, with the frame rate.', module = 'bouncing-bunnies'},
        {code = 'SPR-009', title = 'Render targets', description = 'An offscreen canvas drawn every frame and used as a texture in several ways.', module = 'render-targets'},
        {code = 'SPR-010', title = 'Primitives', description = 'Lines, polylines, rectangles, circles, rings, arcs, polygons and meshes.', module = 'primitives'},
        {code = 'SPR-011', title = 'Text', description = 'Sizes, colors, outlines, shadows, alignment, wrapping, anchors, rotation and measuring.', module = 'text'},
    },
}
