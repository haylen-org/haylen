-- A hundred thousand entries loaded in pages by jobs, with placeholders while a page is on its way, and a number field that scrolls to any entry with the alignment the player picks.
local debug = require('haylen.debug')
local haylen = require('haylen')
local jobs = require('haylen.jobs')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local LongLists = haylen.class('LongLists', Test)

LongLists.count = 100000
LongLists.pageSize = 100

function LongLists:init(entry)
    LongLists.super.init(self, entry)
    self.loads = 0
    self.target = 50000
    self.align = 'center'
end

function LongLists:enter()
    self:frame{
        hint = 'Fling or drag the scrollbar across the hundred thousand entries, or type a number and press Go. Page Up, Page Down, Home, End and the shoulder buttons page through the entries with the focus.',
        focus = 'target',
        content = {self:columns()},
    }
    self.entries = self.gui:collection('entries')
    self.entries:setPages{count = LongLists.count, pageSize = LongLists.pageSize, load = function(first, count)
        return self:loadPage(first, count)
    end}
end

-- Each page builds in a job that shares the frame budget, so a fast fling never stalls a frame, and its entries show placeholders until it resolves.
function LongLists:loadPage(first, count)
    self.loads = self.loads + 1
    return jobs.spawn(function()
        local page = {}
        for index = first, first + count - 1 do
            page[#page + 1] = {id = 'entry-' .. index, type = 'entry', title = 'Entry ' .. index, seed = string.format('Seed %08X', (index * 2654435761) % 4294967296)}
            jobs.checkpoint()
        end
        return page
    end)
end

function LongLists:columns()
    return layout.columns{
        layout.section('A hundred thousand entries', {grow = 3, align = 'stretch',
            ui.collection{id = 'entries', grow = 1, gap = 4, placeholder = 'loading', focusAlign = 'center', types = {
                entry = {template = ui.row{gap = 16,
                    ui.label{part = 'number', bind = {text = '$index'}, width = 140, color = 'textMuted'},
                    ui.column{grow = 1, ui.label{part = 'title', bind = {text = 'title'}}, ui.label{part = 'seed', bind = {text = 'seed'}, font = 'caption', color = 'textMuted'}},
                }, estimatedSize = 76},
                loading = {template = ui.row{gap = 16, height = 76, ui.busyIndicator{size = 32}, ui.label{text = 'Loading', color = 'textMuted'}}, interactive = false},
            }, onSelect = function(event)
                self:set('picked', {text = 'Picked entry ' .. event.index .. '.'})
            end},
        }),
        layout.section('Scroll to an entry', {grow = 1,
            ui.numberField{id = 'target', value = self.target, min = 1, max = LongLists.count, onChange = function(event) self.target = event.value end},
            ui.segmentedControl{id = 'align', items = {{id = 'start', text = 'Start'}, {id = 'center', text = 'Center'}, {id = 'end', text = 'End'}}, selected = self.align, onChange = function(event)
                self.align = event.value
            end},
            ui.button{id = 'go', text = 'Go', variant = 'primary', onClick = function()
                self.entries:scrollTo(math.floor(self.target), {align = self.align})
                self.entries:focus(math.floor(self.target))
            end},
            ui.label{id = 'picked', text = 'Press an entry to pick it.', color = 'textMuted'},
        }),
    }
end

function LongLists:update(dt)
    LongLists.super.update(self, dt)
    local first, last = self.entries:visibleRange()
    local cells = debug.stats().objects.UiCell or {alive = 0, created = 0}
    self:status(string.format('Entries %s to %s of %d, pages loaded %d, cells alive %d, created %d, focus on %s.', tostring(first), tostring(last), self.entries.count, self.loads, cells.alive, cells.created, tostring(self.entries.focusedItem)))
end

return LongLists
