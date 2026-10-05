-- The test library of the engine for Varn `ffi`: its C declarations, made once for the Lua state, and the library, loaded on first use. The browser has no native libraries, so there the plugin `native-test` answers the same functions as its methods.
local ffi = require('ffi')
local native = require('haylen.native')

local library = {}

if native.available() then
    ffi.cdef[[
        typedef struct NativeTestPoint { int32_t x; int32_t y; } NativeTestPoint;
        typedef struct NativeTestRect { NativeTestPoint origin; float width; float height; } NativeTestRect;
        typedef void (*NativeTestVisitor)(int32_t index, const char* label);
        typedef void (*NativeTestReporter)(int32_t value, const uint8_t* data, size_t size);
        int32_t native_test_add(int32_t a, int32_t b);
        double native_test_scale(double value, double factor);
        const char* native_test_origin(void);
        NativeTestPoint native_test_point_add(NativeTestPoint a, NativeTestPoint b);
        void native_test_rect_grow(NativeTestRect* rect, float amount);
        void native_test_fill(uint8_t* buffer, size_t size, uint8_t seed);
        uint32_t native_test_checksum(const uint8_t* buffer, size_t size);
        int32_t native_test_visit(int32_t count, NativeTestVisitor visitor);
        void native_test_report_later(NativeTestReporter reporter, int32_t value);
    ]]
end

function library.load()
    library.loaded = library.loaded or native.load('native_test')
    return library.loaded
end

-- The FNV-1a hash of a Lua string, which the library computes over the same bytes.
function library.checksum(bytes)
    local hash = 2166136261
    for index = 1, #bytes do
        hash = ((hash ~ bytes:byte(index)) * 16777619) & 0xFFFFFFFF
    end
    return hash
end

return library
