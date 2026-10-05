-- C from Lua with Varn's "ffi" module: declarations, calls into the C library of the platform, structs with methods, buffers, pointers and a Lua function that C calls back while it sorts.
local ffi = require('ffi')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local VarnTest = require('categories.varn.varn-test')

local Ffi = haylen.class('Ffi', VarnTest)

Ffi.hint = 'The bars show the scores before and after the C function "qsort" ordered them with the Lua comparator. Run again repeats the lesson.'
Ffi.scores = {50, 10, 40, 20, 30}

-- Declarations and metatypes belong to the Lua state, which raises on a second declaration of a name, so the test makes them once, when its module loads.
ffi.cdef[[
    unsigned long strlen(const char *s);
    void qsort(void *base, unsigned long count, unsigned long size, int (*compare)(const void *, const void *));
    typedef struct { float x, y; int16_t hp; } VarnUnit;
]]

Ffi.Unit = ffi.metatype(ffi.typeof('VarnUnit'), {__index = {
    distance = function(unit)
        return math.sqrt(unit.x * unit.x + unit.y * unit.y)
    end,
}})

Ffi.excerpts = {
    {'Declare and call', [=[
ffi.cdef[[
    unsigned long strlen(const char *s);
    typedef struct { float x, y; int16_t hp; } VarnUnit;
]]
print(ffi.C.strlen('Haylen'))
local unit = ffi.new('VarnUnit', {3, 4, 30})
print(ffi.sizeof('VarnUnit'), ffi.offsetof('VarnUnit', 'hp'))
print(unit:distance())]=]},
    {'Buffers and callbacks', [[
local buffer = ffi.new('uint8_t[?]', 8)
ffi.fill(buffer, 8, 0x41)
ffi.copy(buffer, 'Varn', 4)
print(ffi.string(buffer, 8))

local scores = ffi.new('int32_t[5]', {50, 10, 40, 20, 30})
local compare = ffi.cast('int (*)(const void *, const void *)',
    function(a, b)
        return ffi.cast('const int32_t *', a)[0]
            - ffi.cast('const int32_t *', b)[0]
    end)
ffi.C.qsort(scores, 5, ffi.sizeof('int32_t'), compare)]]},
}

function Ffi:run()
    local length = ffi.C.strlen('Haylen')
    self:check('call', 'Call a C function', length == 6, string.format('The C function "strlen" counted %d bytes in "Haylen".', length))

    local unit = ffi.new('VarnUnit', {3, 4, 30})
    local size, offset = ffi.sizeof('VarnUnit'), ffi.offsetof('VarnUnit', 'hp')
    self:check('struct', 'Structs', size == 12 and offset == 8 and unit.hp == 30 and ffi.istype(Ffi.Unit, unit), string.format('A "VarnUnit" takes %d bytes, with "hp" at byte %d, and the new one holds %g, %g and %d.', size, offset, unit.x, unit.y, unit.hp))
    self:check('methods', 'Methods on C types', unit:distance() == 5, string.format('The method "distance" that "ffi.metatype" gave the struct returned %g.', unit:distance()))

    local buffer = ffi.new('uint8_t[?]', 8)
    ffi.fill(buffer, 8, 0x41)
    ffi.copy(buffer, 'Varn', 4)
    local text = ffi.string(buffer, 8)
    self:check('buffer', 'Buffers', text == 'VarnAAAA', string.format('Filled 8 bytes with 0x41, copied "Varn" over the first 4 and read "%s".', text))

    self:sort()
    self.results:set('version', 'info', 'Version', string.format('The value of "ffi.VERSION" is "%s".', ffi.VERSION))
end

-- Hands C a pointer to a Lua function, which runs on the frame thread every time "qsort" compares two scores.
function Ffi:sort()
    local scores = ffi.new('int32_t[5]', Ffi.scores)
    local calls = 0
    local compare = ffi.cast('int (*)(const void *, const void *)',
        function(a, b)
            calls = calls + 1
            return ffi.cast('const int32_t *', a)[0]
                - ffi.cast('const int32_t *', b)[0]
        end)
    ffi.C.qsort(scores, 5, ffi.sizeof('int32_t'), compare)

    self.sorted = {}
    for index = 0, 4 do
        self.sorted[index + 1] = scores[index]
    end
    local order = table.concat(self.sorted, ', ')
    self:check('callback', 'A Lua function called from C', order == '10, 20, 30, 40, 50', string.format('The C function "qsort" ordered the scores into %s, calling the Lua comparator %d times.', order, calls))

    local pointer = ffi.cast('int32_t *', scores)
    self:check('pointer', 'Pointers', pointer[2] == 30 and pointer ~= ffi.nullptr and ffi.cast('void *', nil) == ffi.nullptr, string.format('A pointer to the scores reads %d at index 2, and a null pointer equals "ffi.nullptr".', pointer[2]))
end

function Ffi:draw(area)
    local top = self.results:draw(area)
    self:drawScores(Ffi.scores, 'Before', top)
    if self.sorted then
        self:drawScores(self.sorted, 'After', top + 70)
    end
end

function Ffi:drawScores(scores, label, y)
    Test.caption(label, 24, y + 20)
    for index, score in ipairs(scores) do
        local x = 140 + (index - 1) * 90
        graphics2d.drawRect({x, y + 50 - score, 60, score}, Test.accent)
        Test.caption(tostring(score), x + 30, y + 50 - score - 2, {size = 16, color = Test.ink, anchor = {0.5, 1}})
    end
end

return Ffi
