#include "egg/renderer/frame_renderer.h"

frame_renderer::frame_renderer(GLFWwindow* window) {
}

frame frame_renderer::begin_frame() {
    return frame{};
}

void frame_renderer::end_frame(frame&& frame) {
}
