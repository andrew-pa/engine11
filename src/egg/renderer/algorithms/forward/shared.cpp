#include "egg/renderer/algorithms/forward.h"

extern "C" rendering_algorithm* create_rendering_algorithm() {
    return new forward_rendering_algorithm;
}
