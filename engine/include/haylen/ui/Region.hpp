#pragma once

#include <memory>
#include <string>

namespace haylen::ui {

class Gui;

// The node of a mounted GUI whose area other GUIs lay out in, such as the middle that the bars of an app leave between them, where the screens of a navigation stack show.
struct Region {
    std::shared_ptr<Gui> gui;
    std::string node;
};

} // namespace haylen::ui
