#include "3d/gltf/AccessorView.hpp"

#include <algorithm>
#include <limits>
#include <string>

namespace haylen::gltf {

AccessorView::AccessorView(const Document& document, std::size_t index) : accessor(&document.accessors.at(index)), components(accessor->getComponentCount()), elementSize(accessor->getElementSize()) {
    const std::size_t componentSize = Accessor::getComponentSize(accessor->componentType);
    const std::size_t rows = Accessor::getColumnRows(accessor->type);
    const std::size_t columnSize = components == rows ? componentSize * rows : (componentSize * rows + 3) / 4 * 4;
    for (std::size_t component = 0; component < components; ++component) {
        componentOffsets[component] = component / rows * columnSize + component % rows * componentSize;
    }

    if (accessor->bufferView) {
        const BufferView& view = document.bufferViews.at(*accessor->bufferView);
        stride = view.byteStride.value_or(static_cast<std::uint32_t>(elementSize));
        base = locateView(document, *accessor->bufferView, accessor->byteOffset, accessor->count, stride, elementSize);
    }

    if (accessor->sparse) {
        const Accessor::Sparse& sparse = *accessor->sparse;
        sparseCount = sparse.count;
        sparseIndexSize = Accessor::getComponentSize(sparse.indicesComponentType);
        if (sparseCount > accessor->count || sparse.indicesComponentType == Accessor::ComponentType::Float) {
            throw std::invalid_argument("The sparse part of an accessor replaces more elements than it holds or has indices that are not integers.");
        }

        sparseIndices = locateView(document, sparse.indicesBufferView, sparse.indicesByteOffset, sparseCount, sparseIndexSize, sparseIndexSize);
        sparseValues = locateView(document, sparse.valuesBufferView, sparse.valuesByteOffset, sparseCount, elementSize, elementSize);
    }
}

bool AccessorView::fits(std::uint64_t offset, std::uint64_t count, std::uint64_t stride, std::uint64_t size, std::uint64_t length) noexcept {
    if (count == 0) {
        return offset <= length;
    }

    constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
    if (stride != 0 && count - 1 > (kMax - size) / stride) {
        return false;
    }

    const std::uint64_t span = (count - 1) * stride + size;
    return offset <= length && span <= length - offset;
}

const std::uint8_t* AccessorView::locateView(const Document& document, std::size_t view, std::uint64_t offset, std::uint64_t count, std::uint64_t step, std::uint64_t size) const {
    const BufferView& bufferView = document.bufferViews.at(view);
    const Buffer& buffer = document.buffers.at(bufferView.buffer);
    const bool viewFits = fits(bufferView.byteOffset, 1, 0, bufferView.byteLength, buffer.data.size());
    if (!viewFits || !fits(offset, count, step, size, bufferView.byteLength)) {
        throw std::invalid_argument("An accessor of the glTF document reads outside the bytes of its buffer view or its buffer.");
    }

    return buffer.data.data() + bufferView.byteOffset + offset;
}

std::uint64_t AccessorView::getSparseIndex(std::uint64_t position) const noexcept {
    const std::uint8_t* entry = sparseIndices + position * sparseIndexSize;
    switch (sparseIndexSize) {
    case 1:
        return *entry;
    case 2: {
        std::uint16_t value = 0;
        std::memcpy(&value, entry, sizeof(value));
        return value;
    }
    default: {
        std::uint32_t value = 0;
        std::memcpy(&value, entry, sizeof(value));
        return value;
    }
    }
}

double AccessorView::normalize(Accessor::ComponentType componentType, double stored) noexcept {
    switch (componentType) {
    case Accessor::ComponentType::Byte:
        return std::max(stored / 127.0, -1.0);
    case Accessor::ComponentType::UnsignedByte:
        return stored / 255.0;
    case Accessor::ComponentType::Short:
        return std::max(stored / 32767.0, -1.0);
    case Accessor::ComponentType::UnsignedShort:
        return stored / 65535.0;
    case Accessor::ComponentType::UnsignedInt:
    case Accessor::ComponentType::Float:
        return stored;
    }
    return stored;
}

} // namespace haylen::gltf
