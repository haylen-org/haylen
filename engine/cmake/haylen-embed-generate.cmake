# Writes OUTPUT as a C++ source that defines the static method SYMBOL of haylen::core::EmbeddedFiles, which returns the bytes of INPUT.

file(READ "${INPUT}" content HEX)
string(LENGTH "${content}" length)
math(EXPR size "${length} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${content}")
string(REGEX REPLACE "(0x[0-9a-f][0-9a-f],){16}" "\\0\n        " bytes "${bytes}")

file(WRITE "${OUTPUT}" "#include \"core/EmbeddedFiles.hpp\"\n\nnamespace haylen::core {\n\nstd::span<const std::uint8_t> EmbeddedFiles::${SYMBOL}() {\n    static constexpr std::uint8_t kData[${size}] = {\n        ${bytes}\n    };\n    return {kData, ${size}};\n}\n\n}\n")
