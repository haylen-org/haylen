-- Developer console: commands of the test with typed arguments that end with it, lines run from code with their results, completions and the history, and the console window that the grave accent key, a touch of four fingers or the button opens.
local debugging = require('haylen.debug')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Console = haylen.class('Console', Test)

function Console:enter()
    self.lines = {}
    self.gold = 0
    self.god = false
    debugging.addCommand('gold', function(amount)
        self.gold = self.gold + amount
        self:set('state', {text = self:describe()})
        return 'Gold ' .. self.gold
    end, {help = 'Adds gold to the player.', args = {'integer'}, owner = self})
    debugging.addCommand('god', function(enabled)
        self.god = enabled
        self:set('state', {text = self:describe()})
        return enabled and 'Damage is off.' or 'Damage is on.'
    end, {help = 'Turns damage off or on.', args = {'boolean'}, owner = self})
    debugging.addCommand('say', function(text)
        self:note('The player says: ' .. text)
    end, {help = 'Prints a text.', args = {'text'}, owner = self})

    self:frame{
        hint = 'Open the console with the grave accent key, four fingers or the button, and type "gold 50", "god true" or "help".',
        content = {ui.row{wrap = true, grow = 1, gap = 24,
            ui.panel{width = 520, align = 'stretch', gap = 12,
                ui.label{id = 'state', text = self:describe(), font = 'heading'},
                ui.button{id = 'open', text = 'Open the console', align = 'stretch', variant = 'primary', onClick = function() self:open() end},
                ui.button{id = 'run', text = 'Run "gold 10" from code', align = 'stretch', onClick = function() self:run('gold 10') end},
                ui.button{id = 'wrong', text = 'Run "gold ten" from code', align = 'stretch', onClick = function() self:run('gold ten') end},
                ui.label{font = 'monospace', text = "debug.addCommand(name, fn, options)\ndebug.runCommand(line)\ndebug.completeCommand(line)\ndebug.setConsoleVisible(true)"},
            },
            ui.scroll{grow = 1, ui.label{id = 'log', text = '', grow = 1}},
        }},
    }
    self:run('gold 50')
    self:run('god true')
    self:run('say Hello "there"')
    self:note('Completions of "go": ' .. table.concat(debugging.completeCommand('go'), ', ') .. '.')
end

function Console:exit()
    debugging.setConsoleVisible(false)
    Console.super.exit(self)
end

function Console:describe()
    return string.format('Gold %d, damage %s', self.gold, self.god and 'off' or 'on')
end

function Console:note(line)
    table.insert(self.lines, 1, line)
    self.lines[13] = nil
    self:set('log', {text = table.concat(self.lines, '\n')})
end

function Console:run(line)
    local ok, output = debugging.runCommand(line)
    self:note(string.format('%s "%s": %s', ok and 'Ran' or 'Refused', line, output ~= '' and output or 'no output.'))
end

function Console:open()
    if not debugging.consoleEnabled() then
        self:note('The console is off, because the app runs outside development without "debug.console" in "app.json".')
        return
    end
    debugging.setConsoleVisible(true)
end

return Console
