-- The tests of the category in menu order.
return {
    prefix = 'EFX',
    title = 'Screen and sprite effects',
    description = 'Camera shake and flashes, distortion, dissolving sprites, outlines and glows, the post-processing chain of world canvases, the effects of images, backdrop blur and glass, effects of your own and four ways to show a character behind trees.',
    tests = {
        {code = 'EFX-001', title = 'Shake and flash', description = 'Hits that shake the camera with trauma that decays, flash the whole view and flash the struck sprite.', module = 'hits'},
        {code = 'EFX-002', title = 'Heat haze and shock rings', description = 'Draws that bend the image: hot air over a fire, shock rings from the cursor, a rippling shield and a glass bubble.', module = 'distortion'},
        {code = 'EFX-003', title = 'Dissolve', description = 'Sprites that dissolve into noise with a colored edge and come back, with the edge width, the cell size and the color on controls.', module = 'dissolve'},
        {code = 'EFX-004', title = 'Outline and glow', description = 'A selection outline on the sprite under the cursor and glows that pulse around the picked sprites.', module = 'outline-glow'},
        {code = 'EFX-005', title = 'Post-processing chain', description = 'Bloom, vignette, saturation, contrast, color grading, chromatic aberration, pixelation, blur, film grain and dithering with presets.', module = 'post-processing'},
        {code = 'EFX-006', title = 'Image effects', description = 'Blur, sharpening, brightness, contrast, saturation, hue, pixelation, inversion and outlines applied to images on the task pool before they become textures.', module = 'image-effects'},
        {code = 'EFX-007', title = 'Backdrop blur', description = 'A region that blurs whatever lies behind it this frame, following the cursor over a moving landscape, with its radius, corners and size on controls.', module = 'backdrop-blur'},
        {code = 'EFX-008', title = 'Glass', description = 'Glass that blurs, saturates, tints, lights its rim and bends its edge over the landscape, and the glass panels, cards and dialogs of the interface.', module = 'glass'},
        {code = 'EFX-009', title = 'Effects of your own', description = 'Two shaders of the project on glass, a mosaic of privacy glass and a lens that magnifies, which read what lies behind them.', module = 'own-effects'},
        {code = 'EFX-010', title = 'Behind the trees', description = 'Four ways to show a character who walks behind trees: the tree fades, turns to a dither, opens a round hole, or the hidden part draws as a silhouette.', module = 'behind-trees'},
    },
}
