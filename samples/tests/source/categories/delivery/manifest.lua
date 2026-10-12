-- The tests of the category in menu order.
return {
    prefix = 'DLV',
    title = 'Content delivery',
    description = 'Assets that the app needs to start, assets that download in the background or when the app asks for them, their progress, and how a release ships and updates its content.',
    tests = {
        {code = 'DLV-001', title = 'Required content', description = 'An asset that is on the device before the app starts, read at once, and how the app runs from a protected release.', module = 'required'},
        {code = 'DLV-002', title = 'On-demand content', description = 'An asset that comes only when the app asks for it: a synchronous load refuses it, and an asynchronous load brings it first.', module = 'on-demand'},
        {code = 'DLV-003', title = 'Prefetch and progress', description = 'Assets brought onto the device without loading them, with the progress of the bytes that arrive.', module = 'prefetch'},
        {code = 'DLV-004', title = 'Updates and offline play', description = 'The status of the content, the update events, a check that fails and the content that plays when nothing can download.', module = 'updates'},
    },
}
