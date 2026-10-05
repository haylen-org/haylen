-- The tests of the category in menu order.
return {
    prefix = 'AUD',
    title = 'Audio',
    description = 'Music, effects, buses, filters, positional sound, interruptions and many voices with sounds synthesized for these tests.',
    tests = {
        {code = 'AUD-001', title = 'Music', description = 'Two streamed tracks with crossfades of any length, looping or played once, started partway, paused, resumed and moved through with the cursor of their voice.', module = 'music'},
        {code = 'AUD-002', title = 'One-shot effects', description = 'Volume, pitch, pitch variation, pan, fades and a limit of voices per sound.', module = 'one-shots'},
        {code = 'AUD-003', title = 'Buses', description = 'The bus tree with volumes, mutes and process modes, and what the game pause stops.', module = 'buses'},
        {code = 'AUD-004', title = 'Effects', description = 'Every filter, the delay and the reverb on a bus or on a voice, with tweened parameters.', module = 'effects'},
        {code = 'AUD-005', title = 'Positional audio', description = 'Sounds in the world around a listener that follows the camera, with fade models, panning and Doppler.', module = 'positional'},
        {code = 'AUD-006', title = 'Interruptions and lifecycle', description = 'Audio interruptions, route changes and app states as they happen, and what the background does to the sound.', module = 'lifecycle'},
        {code = 'AUD-007', title = 'Many voices', description = 'Bursts and rain of voices up to the limit of 128, with the statistics of every bus.', module = 'stress'},
    },
}
