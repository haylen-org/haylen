-- The tests of the category in menu order.
return {
    prefix = 'EFX',
    title = 'Screen and sprite effects',
    description = 'Camera shake and flashes, distortion, dissolving sprites, outlines and glows, the post-processing chain of world canvases and the effects of images.',
    tests = {
        {code = 'EFX-001', title = 'Shake and flash', description = 'Hits that shake the camera with trauma that decays, flash the whole view and flash the struck sprite.', module = 'hits'},
        {code = 'EFX-002', title = 'Heat haze and shock rings', description = 'Draws that bend the image: hot air over a fire, shock rings from the cursor, a rippling shield and a glass bubble.', module = 'distortion'},
        {code = 'EFX-003', title = 'Dissolve', description = 'Sprites that dissolve into noise with a colored edge and come back, with the edge width, the cell size and the color on controls.', module = 'dissolve'},
        {code = 'EFX-004', title = 'Outline and glow', description = 'A selection outline on the sprite under the cursor and glows that pulse around the picked sprites.', module = 'outline-glow'},
        {code = 'EFX-005', title = 'Post-processing chain', description = 'Bloom, vignette, saturation, contrast, color grading, chromatic aberration, pixelation, blur, film grain and dithering with presets.', module = 'post-processing'},
        {code = 'EFX-006', title = 'Image effects', description = 'Blur, sharpening, brightness, contrast, saturation, hue, pixelation, inversion and outlines applied to images on the task pool before they become textures.', module = 'image-effects'},
    },
}
