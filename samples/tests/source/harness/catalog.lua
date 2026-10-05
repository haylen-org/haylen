-- Every category of the project in menu order, grouped by section, with the tests that the manifest of each category folder lists. A manifest returns its `prefix`, `title`, `description` and `tests`, and each test has a `code`, a `title`, a `description`, the `module` of its file in the folder, and optionally the `platforms` it runs on with the `unsupported` reason of every other platform.
local haylen = require('haylen')

local catalog = {}

catalog.platforms = {'macos', 'windows', 'linux', 'ios', 'tvos', 'android', 'web', 'headless'}

catalog.sections = {
    {title = 'Graphics', folders = {'sprites', 'camera', 'text', 'lighting', 'nine-slice', 'particles', 'scenes', 'shaders'}},
    {title = 'Gameplay', folders = {'algorithms', 'audio', 'events', 'input', 'physics', 'tiled', 'tween'}},
    {title = 'Interface', folders = {'interface', 'orientation', 'safe-area'}},
    {title = 'System', folders = {'development', 'files', 'localization', 'native', 'network', 'platform', 'plugins', 'preferences', 'varn'}},
}

catalog.categories = {}
catalog.tests = {}
catalog.byCode = {}

-- Checks a test of a manifest, so a mistake in a manifest stops the project at start with the code and the field that is wrong.
function catalog.check(category, test)
    local code = test.code
    if type(code) ~= 'string' or not code:match('^' .. category.prefix .. '%-%d%d%d$') then
        error(string.format('The test "%s" of the category "%s" needs a code like "%s-001".', tostring(code), category.folder, category.prefix), 0)
    end
    if catalog.byCode[code] then
        error(string.format('The code "%s" is used by more than one test.', code), 0)
    end
    for _, field in ipairs({'title', 'description', 'module'}) do
        if type(test[field]) ~= 'string' or test[field] == '' then
            error(string.format('The test "%s" needs a "%s".', code, field), 0)
        end
    end

    local runs = {}
    for _, platform in ipairs(test.platforms or catalog.platforms) do
        runs[platform] = true
    end
    for _, platform in ipairs(catalog.platforms) do
        if not runs[platform] and not (test.unsupported and test.unsupported[platform]) then
            error(string.format('The test "%s" does not run on "%s" and needs the reason in "unsupported".', code, platform), 0)
        end
    end
end

function catalog.load()
    for _, section in ipairs(catalog.sections) do
        section.categories = {}
        for _, folder in ipairs(section.folders) do
            local category = require('categories.' .. folder .. '.manifest')
            category.folder = folder
            category.section = section
            for _, test in ipairs(category.tests) do
                catalog.check(category, test)
                test.category = category
                test.module = 'categories.' .. folder .. '.' .. test.module
                catalog.byCode[test.code] = test
                catalog.tests[#catalog.tests + 1] = test
            end
            section.categories[#section.categories + 1] = category
            catalog.categories[#catalog.categories + 1] = category
        end
    end
end

-- Returns the reason the test cannot run on this platform, or nothing when it runs here.
function catalog.unsupported(test)
    if not test.platforms then
        return nil
    end
    for _, platform in ipairs(test.platforms) do
        if platform == haylen.platform then
            return nil
        end
    end
    return test.unsupported[haylen.platform]
end

-- Returns the tests whose code or title contains every word of the query, ignoring case.
function catalog.search(query)
    local words = {}
    for word in query:lower():gmatch('%S+') do
        words[#words + 1] = word
    end
    local found = {}
    for _, test in ipairs(catalog.tests) do
        local text = (test.code .. ' ' .. test.title):lower()
        local matches = true
        for _, word in ipairs(words) do
            if not text:find(word, 1, true) then
                matches = false
                break
            end
        end
        if matches then
            found[#found + 1] = test
        end
    end
    return found
end

return catalog
