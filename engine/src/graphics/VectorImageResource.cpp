#include "graphics/VectorImageResource.hpp"

namespace haylen::graphics {

debug::ObjectCounter& VectorImageResource::counter = *new debug::ObjectCounter("VectorImage", debug::ObjectCounter::Kind::Native);
std::atomic<std::uint32_t> VectorImageResource::nextId{1};

} // namespace haylen::graphics
