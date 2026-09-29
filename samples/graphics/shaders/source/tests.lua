-- The tests of the sample in menu order. Each module returns a scene class that takes its entry.
return {
    {id = 'materials', title = 'Sprite materials', description = 'Dissolve, outline, hit flash, wave distortion, palette swap, hologram and pixelation shaders on sprites.', module = 'tests.materials'},
    {id = 'tweened-uniforms', title = 'Uniforms with tweens', description = 'Shader values driven by tweens, yoyos, elastic curves and a timeline.', module = 'tests.tweened-uniforms'},
    {id = 'texture-uniforms', title = 'Textures as uniforms', description = 'A material that reads a gradient and a pattern texture, including a live render target.', module = 'tests.texture-uniforms'},
    {id = 'post-processing', title = 'Post-processing chain', description = 'The built-in vignette followed by custom blur, color grading and CRT passes over a canvas.', module = 'tests.post-processing'},
    {id = 'text-shader', title = 'Shaders on text', description = 'Rainbow, dissolve and flash materials on text drawn from its distance field.', module = 'tests.text-shader'},
    {id = 'hot-reload', title = 'Hot reload', description = 'How a desktop dev run recompiles and reloads a shader while the app keeps running.', module = 'tests.hot-reload'},
}
