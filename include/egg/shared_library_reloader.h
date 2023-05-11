#pragma once
#include <functional>
#include "fs-shim.h"
#include "dl-shim.h"

class shared_library_reloader {
    std::filesystem::path library_path;
    uint64_t reload_counter;
    void* current_lib;
    void* watcher;
    std::atomic_bool should_reload;
public:
    shared_library_reloader(std::filesystem::path library_path);
    ~shared_library_reloader();

    shared_library_reloader(shared_library_reloader&) = delete;
    shared_library_reloader& operator=(shared_library_reloader&) = delete;

    void* initial_load();

    void poll(std::function<void(void*)> on_reload);
};
