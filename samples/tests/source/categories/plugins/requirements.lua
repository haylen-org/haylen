-- The plugin calls a method that needs something the project of the app lacks on purpose, such as a permission that the Android manifest does not declare. The native part checks the requirement before it calls the system, logs once what is missing and how to add it, and fails the call with the code `unsupported`, whose `data.missing` lists each missing requirement with the file and the snippet that add it, instead of crashing the app.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Requirements = haylen.class('Requirements', DemoTest)

function Requirements:enter()
    self:frame{
        hint = 'Check the requirement, which the project lacks until it adds the snippet the test shows.',
        focus = 'check',
        controls = {
            ui.button{id = 'check', text = 'Check the requirement', variant = 'primary', onClick = function() self:check() end},
            ui.label{text = 'The plugin needs what it leaves out of the project on purpose: the permission "android.permission.READ_CONTACTS" on Android, the usage description "NSContactsUsageDescription" on Apple platforms, and the Contact Picker API "navigator.contacts" on the web, which only browsers of phones offer. The call fails with the code "unsupported" instead of crashing the app, and the log tells once what is missing.', color = 'textMuted', font = 'caption'},
        },
        code = "local _, err = demo.requirementCheck():await()\nfor _, missing in ipairs(err.data.missing) do\n  print(missing.file, missing.snippet)\nend",
    }
end

function Requirements:check()
    self:act(function()
        self.results:clear()
        local name = 'The call "requirementCheck"'
        self.results:set('call', 'waiting', name, 'The native part checks what the project of the app holds.')
        local answer, err = demo.requirementCheck():await()
        if not err then
            self.results:set('call', 'pass', name, string.format('The project has what the native part in %s needs, so the call answered.', answer.language))
            return
        elseif err.code == 'noHandler' then
            self.results:set('call', 'skip', name, 'The native part of this platform checks no requirements.')
            return
        elseif err.code ~= 'unsupported' or not (err.data and err.data.missing) then
            self.results:failure('call', name, err)
            return
        end

        self.results:set('call', 'pass', name, 'The call failed with the code "unsupported": ' .. err.message)
        -- A requirement of the web page has no file of a project, and only a feature of the permissions policy has a snippet that allows it.
        for index, missing in ipairs(err.data.missing) do
            local detail = 'The page lacks it, as the log explains.'
            if missing.file ~= '' then
                detail = string.format('Add "%s" to "%s".', missing.snippet, missing.file)
            elseif missing.snippet ~= '' then
                detail = string.format('Allow it with "%s".', missing.snippet)
            end
            self.results:set('missing' .. index, 'info', string.format('Missing %s "%s"', missing.kind, missing.name), detail)
        end
    end)
end

return Requirements
