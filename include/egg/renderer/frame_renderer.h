#pragma once
#include <GLFW/glfw3.h>

struct frame {
};

class frame_renderer {
public:
    frame_renderer(GLFWwindow* window);

    frame begin_frame();
    void end_frame(frame&& frame);
};
