#include "egg/renderer/algorithms/forward.h"

extern "C" SHARED_EXPORT rendering_algorithm * create_rendering_algorithm() {
    return new forward_rendering_algorithm;
}
