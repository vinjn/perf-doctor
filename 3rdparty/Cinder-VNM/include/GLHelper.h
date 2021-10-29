#pragma once

#include "cinder/gl/gl.h"
#include "cinder/app/App.h"
#include "cinder/ip/Flip.h"

#if defined( CINDER_GL_ES )
namespace cinder {
    namespace gl {
        void enableWireframe() {}
        void disableWireframe() {}
        void setWireframeEnabled(bool enable = true) { if (enable) enableWireframe(); else disableWireframe(); }
    }
}
#endif

ci::Surface copyWindowSurfaceWithAlpha() {
    auto area = ci::app::getWindowBounds();
    ci::Surface s(area.getWidth(), area.getHeight(), true);
    glFlush();
    GLint oldPackAlignment;
    glGetIntegerv(GL_PACK_ALIGNMENT, &oldPackAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(area.x1, ci::app::getWindowHeight() - area.y2, area.getWidth(), area.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, s.getData());
    glPixelStorei(GL_PACK_ALIGNMENT, oldPackAlignment);
    ci::ip::flipVertical(&s);
    return s;
}
