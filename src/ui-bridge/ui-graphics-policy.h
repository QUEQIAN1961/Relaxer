#pragma once
class QQuickWindow;
namespace relaxer {
// Configure only before scene graph initialization. No media objects are touched.
void configureUiGraphics(QQuickWindow *window,bool accelerated);
}
