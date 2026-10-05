-- The tests of the category in menu order.
return {
    prefix = 'EVT',
    title = 'Events',
    description = 'Signals, the event bus, owners that end their listeners, scene scopes, the engine lifecycle, the pause, autoloads and classes.',
    tests = {
        {code = 'EVT-001', title = 'Signals', description = 'Connect, once, priority, deferred, blocking and disconnecting during an emit.', module = 'signals'},
        {code = 'EVT-002', title = 'Owners', description = 'Listeners that disconnect by themselves when the object or GUI that owns them ends.', module = 'owners'},
        {code = 'EVT-003', title = 'Event bus', description = 'Channels, filters, priorities, consumed events and events queued for the end of the frame.', module = 'bus'},
        {code = 'EVT-004', title = 'Scene scopes', description = 'Listeners, timers, tweens, tasks and GUIs of a scene that all end when it unloads.', module = 'scopes'},
        {code = 'EVT-005', title = 'Lifecycle log', description = 'Every engine event as it happens: app states, scene loads and transitions, window, assets, objects and sockets.', module = 'lifecycle'},
        {code = 'EVT-006', title = 'Pause', description = 'A pause menu that stops the game, and the "paused" and "unpaused" hooks and events.', module = 'pause'},
        {code = 'EVT-007', title = 'Autoloads', description = 'A player data singleton that lives through every scene, and an autoload added at run time.', module = 'autoloads'},
        {code = 'EVT-008', title = 'Classes', description = 'The class helper "haylen.class" with inheritance, "super" calls, metamethods, "is" checks and mixins.', module = 'classes'},
        {code = 'EVT-009', title = 'Diagnostics', description = 'The live counts of "events.topics()" and "signal.list()", and the counts around the collection of an owner.', module = 'diagnostics'},
    },
}
