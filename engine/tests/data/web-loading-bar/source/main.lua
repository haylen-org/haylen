-- The app of the loading check of the web whose page has no "#splash-logo", while `haylen test --target web` checks that its splash shows the bar alone and that its percentage grows to 100%.
local log = require('haylen.log')
local scene = require('haylen.scene')

log.info('The loading check without a logo is ready.')
scene.push({})
