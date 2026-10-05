-- Varn's "log" and "platform" modules: leveled lines with structured fields that reach the log of the engine, and what Varn reports about the device and itself.
local debug = require('haylen.debug')
local engineLog = require('haylen.log')
local haylen = require('haylen')
local log = require('log')
local platform = require('platform')

local Test = require('harness.test')
local VarnTest = require('categories.varn.varn-test')

local LogPlatform = haylen.class('LogPlatform', VarnTest)

LogPlatform.colors = {info = Test.ink, warning = Test.warm, error = Test.red}

LogPlatform.hint = 'The lesson reads its own lines back from "debug.recentLog", which keeps the recent lines of the app, the engine and Varn. Run again repeats the lesson.'

LogPlatform.excerpts = {
    {'Log', [[
log.info('Chest opened', {gold = 12})
log.warn('Low tide', {depth = 2})
log.debug('Below the level, so dropped.')
log.setLevel('warn')
log.info('Dropped while the level is "warn".')
log.setLevel('info')
for _, line in ipairs(debug.recentLog(10)) do
    print(line.level, line.text)
end]]},
    {'Platform', [[
print(platform.os(), platform.arch())
print(platform.cpuCount(), platform.pointerSize())
print(platform.endianness(), platform.hostVersion())
print(platform.libraryFilename('native_test'))]]},
}

function LogPlatform:run()
    self:logLines()
    self:system()
end

-- Writes at fixed levels and restores the level of the app at the end, which "haylen.log.setLevel" sets for the engine and Varn alike.
function LogPlatform:logLines()
    local previous = engineLog.level()
    log.setLevel('info')
    log.info('Chest opened', {gold = 12})
    log.warn('Low tide', {depth = 2})
    log.debug('Below the level, so dropped.')
    log.setLevel('warn')
    log.info('Dropped while the level is "warn".')
    log.setLevel('info')
    log.info('Written again at the level "info".')
    engineLog.setLevel(previous)

    local opened, tide = self:find('Chest opened'), self:find('Low tide')
    self.lines = {opened, tide, self:find('Written again at the level')}
    self:check('fields', 'Structured fields', opened ~= nil and opened.text:find('Chest opened gold=12', 1, true) ~= nil, string.format('The engine log got "%s".', opened and opened.text or 'nothing'))
    self:check('levels', 'Levels', tide ~= nil and tide.level == 'warning' and tide.text:find('depth=2', 1, true) ~= nil, string.format('The line of "log.warn" arrived at the level "%s" of the engine.', tide and tide.level or 'none'))
    self:check('debug', 'Below the level', self:find('Below the level, so dropped.') == nil, 'The function "log.debug" wrote nothing, since the level was "info".')
    self:check('raised', 'Raising the level', self:find('Dropped while the level is') == nil and self:find('Written again at the level') ~= nil, 'The function "log.info" wrote nothing while the level was "warn", and wrote again once it was back at "info".')
end

-- Returns the newest line of the recent log that contains `text`.
function LogPlatform:find(text)
    local lines = debug.recentLog(20)
    for index = #lines, 1, -1 do
        if lines[index].text:find(text, 1, true) then
            return lines[index]
        end
    end
    return nil
end

function LogPlatform:system()
    local cpus, pointer, order = platform.cpuCount(), platform.pointerSize(), platform.endianness()
    self:check('system', 'The device', cpus >= 1 and (pointer == 4 or pointer == 8), string.format('Varn runs on "%s" with the architecture "%s", %d %s, %d-byte pointers and %s-endian numbers.', platform.os(), platform.arch(), cpus, cpus == 1 and 'processor' or 'processors', pointer, order))

    local version = platform.version
    self:check('version', 'The version of Varn', platform.hostVersion() == version.string, string.format('Varn %s, which "platform.version" splits into %d, %d and %d.', platform.hostVersion(), version.major, version.minor, version.patch))

    self.results:set('library', 'info', 'Library names', string.format('A library named "native_test" is the file "%s" here, from the prefix "%s" and the suffix "%s".', platform.libraryFilename('native_test'), platform.libPrefix(), platform.shlibSuffix()))
    self.results:set('names', 'info', 'Two names for the platform', string.format('Varn says "%s" and "haylen.platform" says "%s", because the engine gives its own names to platforms such as the headless host and the browser.', platform.os(), haylen.platform))
end

-- Draws the lines of the lesson the way the engine log keeps them.
function LogPlatform:draw(area)
    local y = self.results:draw(area)
    Test.caption('The lines of the lesson in the engine log', 24, y)
    for _, line in ipairs(self.lines or {}) do
        y = y + 30
        Test.caption(string.format('%s%s  %s', line.level:sub(1, 1):upper(), line.level:sub(2), line.text), 24, y, {size = 17, color = LogPlatform.colors[line.level] or Test.ink, maxWidth = area.width - 48})
    end
end

return LogPlatform
