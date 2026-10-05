-- Commands with Varn's "process" module: their output, their errors and exit codes, a deadline that kills a command, and the environment of the app.
local datetime = require('datetime')
local haylen = require('haylen')
local process = require('process')

local VarnTest = require('categories.varn.varn-test')

local Processes = haylen.class('Processes', VarnTest)

Processes.hint = 'Every command runs through the shell of the platform on the I/O pool, so the frame never waits for it. Run again repeats the lesson.'
-- A command that runs for 5 seconds in the shell of the platform, "cmd.exe" on Windows and "sh" elsewhere.
Processes.slow = haylen.platform == 'windows' and 'ping -n 6 127.0.0.1 > nul' or 'sleep 5'

Processes.excerpts = {
    {'Commands', [[
local result = process.exec('echo hello'):await()
print(result.code, result.stdout)
local failed = process.exec('echo oops 1>&2 && exit 3'):await()
print(failed.code, failed.stderr)
local _, failure = process.exec('sleep 5', {
    timeoutMs = 300,
}):await()]]},
    {'Environment', [[
print(process.available, process.cwd())
print(process.getenv('PATH'))
print(process.getenv('HAYLEN_MISSING', 'none'))
print(#process.argv)]]},
}

function Processes:run()
    self:check('available', 'Available', process.available, string.format('The value of "process.available" is %s on "%s".', tostring(process.available), haylen.platform))

    local started = datetime.now():millis()
    local result, failure = process.exec('echo hello'):await()
    if not result then
        self:check('run', 'Run a command', false, 'The command failed: ' .. tostring(failure))
        return
    end
    local output = result.stdout:gsub('%s+$', '')
    self:check('run', 'Run a command', result.code == 0 and output == 'hello', string.format('The command "echo hello" printed "%s" and ended with code %d after %d ms.', output, result.code, datetime.now():millis() - started))

    local failed = process.exec('echo oops 1>&2 && exit 3'):await()
    local errors = failed.stderr:gsub('%s+$', '')
    self:check('code', 'Errors and exit codes', failed.code == 3 and errors == 'oops' and failed.stdout == '', string.format('Ended with code %d, with "%s" on the error stream and nothing on the output.', failed.code, errors))

    self.results:set('deadline', 'waiting', 'A deadline', string.format('Waiting at most 300 ms for "%s".', Processes.slow))
    started = datetime.now():millis()
    local late, reason = process.exec(Processes.slow, {
        timeoutMs = 300,
    }):await()
    self:check('deadline', 'A deadline', late == nil and reason ~= nil, string.format('The command "%s" was killed after %d ms, and the promise rejected: %s', Processes.slow, datetime.now():millis() - started, tostring(reason)))

    self:environment()
end

function Processes:environment()
    local path = process.getenv('PATH') or ''
    local separator = haylen.platform == 'windows' and ';' or ':'
    local folders = select(2, path:gsub(separator, '')) + 1
    local missing = process.getenv('HAYLEN_MISSING', 'none')
    self:check('env', 'Environment', path ~= '' and missing == 'none' and type(process.env) == 'table', string.format('The variable "PATH" lists %d folders, and "HAYLEN_MISSING" falls back to "%s".', folders, missing))
    self.results:set('cwd', 'info', 'Working folder and arguments', string.format('The app runs in "%s" with %d %s in "process.argv".', process.cwd(), #process.argv, #process.argv == 1 and 'argument' or 'arguments'))
end

return Processes
