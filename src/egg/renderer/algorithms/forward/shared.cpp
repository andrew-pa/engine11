#include "egg/renderer/algorithms/forward.h"

#ifdef _MSC_VER
#define SHARED_EXPORT __declspec(dllexport) 
#else
#define SHARED_EXPORT
#endif

extern "C" SHARED_EXPORT rendering_algorithm * create_rendering_algorithm() {
    return new forward_rendering_algorithm;
}
