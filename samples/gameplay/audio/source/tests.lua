-- The tests of the sample in menu order. Each module returns the scene class of its test.
return {
    {id = 'music', title = 'Music', description = 'Two streamed tracks with crossfades of any length, looping or played once, paused and resumed.', module = 'tests.music'},
    {id = 'one-shots', title = 'One-shot effects', description = 'Volume, pitch, pitch variation, pan, fades and a limit of voices per sound.', module = 'tests.one-shots'},
    {id = 'buses', title = 'Buses', description = 'The bus tree with volumes, mutes and process modes, and what the game pause stops.', module = 'tests.buses'},
    {id = 'effects', title = 'Effects', description = 'Every filter, the delay and the reverb on a bus or on a voice, with tweened parameters.', module = 'tests.effects'},
    {id = 'positional', title = 'Positional audio', description = 'Sounds in the world around a listener that follows the camera, with fade models, panning and Doppler.', module = 'tests.positional'},
    {id = 'lifecycle', title = 'Interruptions and lifecycle', description = 'Audio interruptions, route changes and app states as they happen, and what the background does to the sound.', module = 'tests.lifecycle'},
    {id = 'stress', title = 'Many voices', description = 'Bursts and rain of voices up to the limit of 128, with the statistics of every bus.', module = 'tests.stress'},
}
