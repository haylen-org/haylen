-- The tests of the sample in menu order. Each module returns the scene class of its test.
return {
    {id = 'basics', title = 'Sprite basics', description = 'Pivot, rotation, scale, flips, tint and flash on one sprite.', module = 'tests.basics'},
    {id = 'atlases', title = 'Atlases and sheets', description = 'A TexturePacker atlas with trimmed frames, an Aseprite atlas with tags and a slice, and a grid sheet.', module = 'tests.atlases'},
    {id = 'animation', title = 'Animation', description = 'Clips, loop modes, frame and finish events and a queue of clips on an animator.', module = 'tests.animation'},
    {id = 'batches', title = 'Sprite batches', description = 'Many sprites of one texture kept in C++ and drawn in one batch, added, changed and removed.', module = 'tests.batches'},
    {id = 'static-batches', title = 'Static batches', description = 'A tile map baked once to the GPU and drawn every frame without uploads, with parallax offsets.', module = 'tests.static-batches'},
    {id = 'layers', title = 'Layers and y-sort', description = 'Walkers among trees sorted by the y they stand on, and a selected walker raised above every layer.', module = 'tests.layers'},
    {id = 'bullets', title = 'Pooled bullets', description = 'A bullet hell whose projectiles come from an object pool and draw in one batch.', module = 'tests.bullets'},
    {id = 'bunnymark', title = 'Bunnymark', description = 'From a few to many sprites with float buffers and drawBatch fields, with the frame rate.', module = 'tests.bunnymark'},
    {id = 'render-targets', title = 'Render targets', description = 'An offscreen canvas drawn every frame and used as a texture in several ways.', module = 'tests.render-targets'},
    {id = 'primitives', title = 'Primitives', description = 'Lines, polylines, rectangles, circles, rings, arcs, polygons and meshes.', module = 'tests.primitives'},
    {id = 'text', title = 'Text', description = 'Sizes, colors, outlines, shadows, alignment, wrapping, anchors, rotation and measuring.', module = 'tests.text'},
}
