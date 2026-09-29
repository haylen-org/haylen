-- The tests of the sample in menu order. Each module returns the scene class of its test.
return {
    {id = 'signals', title = 'Signals', description = 'Connect, once, priority, deferred, blocking and disconnecting during an emit.', module = 'tests.signals'},
    {id = 'owners', title = 'Owners', description = 'Listeners that disconnect by themselves when the object or document that owns them ends.', module = 'tests.owners'},
    {id = 'bus', title = 'Event bus', description = 'Channels, filters, priorities, consumed events and events queued for the end of the frame.', module = 'tests.bus'},
    {id = 'scopes', title = 'Scene scopes', description = 'Listeners, timers, tweens, tasks and documents of a scene that all end when it unloads.', module = 'tests.scopes'},
    {id = 'lifecycle', title = 'Lifecycle log', description = 'Every engine event as it happens: app states, scene loads and transitions, window, assets, objects and sockets.', module = 'tests.lifecycle'},
    {id = 'pause', title = 'Pause', description = 'A pause menu that stops the game, and the paused and unpaused hooks and events.', module = 'tests.pause'},
    {id = 'autoloads', title = 'Autoloads', description = 'A player data singleton that lives through every scene, and an autoload added at run time.', module = 'tests.autoloads'},
    {id = 'classes', title = 'Classes', description = 'haylen.class with inheritance, super calls, metamethods, is checks and mixins.', module = 'tests.classes'},
    {id = 'diagnostics', title = 'Diagnostics', description = 'The live counts of events.topics() and signal.list(), and the counts around the collection of an owner.', module = 'tests.diagnostics'},
}
