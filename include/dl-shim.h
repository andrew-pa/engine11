#pragma once

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
inline void* open_shared_library(const std::filesystem::path& p) {
    return (void*)LoadLibraryW(p.c_str());
}

inline void* load_symbol(void* shared_library, const char* symbol) {
    return GetProcAddress((HINSTANCE)shared_library, symbol);
}

inline void close_shared_library(void* shared_library) {
    FreeLibrary((HINSTANCE)shared_library);
}
#else
#include <dlfcns.h>
inline void* open_shared_library(const std::filesystem::path& p) {
    return dlopen(p.c_str(), RTLD_LAZY|RTLD_LOCAL);
}

inline void* load_symbol(void* shared_library, const char* symbol) {
    return dlsym(shared_library, symbol);
}

inline void close_shared_library(void* shared_library) {
    dlclose(shared_library);
}
#endif
