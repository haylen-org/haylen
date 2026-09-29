#pragma once

#include "ImGuiAssert.hpp"

#define IM_ASSERT(expression) ((expression) ? static_cast<void>(0) : ::haylen::ui::ImGuiAssert::fail(#expression, __FILE__, __LINE__))
