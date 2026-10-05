-- The tests of the category in menu order.
return {
    prefix = 'SHD',
    title = 'Shaders',
    description = 'Custom fragment shaders compiled into ".shader" files: materials on sprites and text, uniforms from tweens and textures, post-processing chains and hot reload.',
    tests = {
        {code = 'SHD-001', title = 'Sprite materials', description = 'Dissolve, outline, hit flash, wave distortion, palette swap, hologram and pixelation shaders on sprites.', module = 'materials'},
        {code = 'SHD-002', title = 'Uniforms with tweens', description = 'Shader values driven by tweens, yoyos, elastic curves and a timeline.', module = 'tweened-uniforms'},
        {code = 'SHD-003', title = 'Textures as uniforms', description = 'A material that reads a gradient and a pattern texture, including a live render target.', module = 'texture-uniforms'},
        {code = 'SHD-004', title = 'Post-processing chain', description = 'The built-in vignette followed by custom blur, color grading and tube screen passes over a canvas.', module = 'post-processing'},
        {code = 'SHD-005', title = 'Shaders on text', description = 'Rainbow, dissolve and flash materials on text drawn from its distance field.', module = 'text-shader'},
        {code = 'SHD-006', title = 'Hot reload', description = 'How a desktop run of the player recompiles and reloads a shader while the app keeps running.', module = 'hot-reload'},
    },
}
