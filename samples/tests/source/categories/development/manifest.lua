-- The tests of the category in menu order.
return {
    prefix = 'DEV',
    title = 'Development',
    description = 'Hot reload of Lua modules in place with the state of the app, their hooks and kept values, the error screen that a saved fix resumes from, and assets that reload while the app runs.',
    tests = {
        {code = 'DEV-001', title = 'Module reload', description = 'A greeting from a module that reloads in place while a counter keeps counting.', module = 'module-reload'},
        {code = 'DEV-002', title = 'Classes', description = 'Walkers of a class that take its new methods in place, with their positions kept.', module = 'classes'},
        {code = 'DEV-003', title = 'Callbacks', description = 'Timers with a module function, a closure made at run time and a call through a table, after a reload.', module = 'callbacks'},
        {code = 'DEV-004', title = 'Hooks and kept values', description = 'The reloaded hook, a kept signal whose listener never doubles and the reloads the event bus reports.', module = 'hooks'},
        {code = 'DEV-005', title = 'Resume after a fix', description = 'An error in an update that saving the fix resumes from, with the state of the test kept.', module = 'resume'},
        {code = 'DEV-006', title = 'Assets', description = 'JSON and the texts of a language that reload in place, with the asset events of each save.', module = 'assets'},
    },
}
