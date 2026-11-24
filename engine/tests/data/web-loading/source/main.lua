-- The app of the loading check of the web, whose page has the splash of the template, while `haylen test --target web` follows the percentage of the splash and the order in which the page downloads its logo, its runtime and its package.
local log = require('haylen.log')
local scene = require('haylen.scene')

log.info('The loading check is ready.')
scene.push({})
