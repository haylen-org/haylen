#include "haylen/core/LoadingView.hpp"

namespace haylen::core {

void LoadingView::enter(Engine&) {}

void LoadingView::exit(Engine&) {}

void LoadingView::update(Engine&, float, const SceneLoad::Progress&) {}

void LoadingView::render(Engine&, const SceneLoad::Progress&) {}

void LoadingView::renderUi(Engine&, const SceneLoad::Progress&) {}

} // namespace haylen::core
