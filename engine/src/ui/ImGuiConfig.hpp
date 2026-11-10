#pragma once

#include "ImGuiAssert.hpp"

// ImGui reads fonts through the HarfBuzz loader of the engine, so the reader it ships, which trusts the bytes of a font, stays out of the build.
#define IMGUI_DISABLE_STB_TRUETYPE

#define IM_ASSERT(expression) ((expression) ? static_cast<void>(0) : ::haylen::ui::ImGuiAssert::fail(#expression, __FILE__, __LINE__))
