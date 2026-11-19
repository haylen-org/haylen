-- Haylen Tests: every feature test of the engine, in a menu of categories, with a code per test. The headless platform runs every test by itself, and the launch arguments "--test <code>" open one test, the way the UI tests do.
local haylen = require('haylen')
local scene = require('haylen.scene')

local catalog = require('harness.catalog')
local navigation = require('harness.navigation')

catalog.load()
navigation.install()
navigation.useActions(nil)

if haylen.platform == 'headless' then
    require('harness.runner').start()
    return
end

local launched = navigation.launchedTest()
if launched then
    navigation.openLaunched(launched)
else
    scene.push(require('harness.menu')())
end
