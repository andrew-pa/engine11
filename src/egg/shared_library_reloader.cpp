#include "egg/shared_library_reloader.h"
#include <iostream>
#include <thread>

#ifdef _MSC_VER
#elif __APPLE__

#else
#    include <linux/limits.h>
#    include <sys/inotify.h>
#    include <unistd.h>

struct inotify_watcher : public abstract_watcher {
    int         fd, wd;
    std::string filename;

    inotify_watcher(const std::filesystem::path& watched_file) : filename(watched_file.filename()) {
        fd = inotify_init1(IN_NONBLOCK);
        if(fd < 0) {
            perror("create inotify fd");
            throw std::runtime_error("failed to create inotify file descriptor");
        }
        std::cout << "watching " << watched_file << "\n";
        wd = inotify_add_watch(fd, watched_file.parent_path().c_str(), IN_CLOSE_WRITE | IN_MODIFY);
        if(wd < 0) {
            perror("create inotify wd");
            throw std::runtime_error(
                std::string("failed to create inotify watch descriptor for ")
                + watched_file.string()
            );
        }
    }

    bool poll() override {
        char event_buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
        int  res = read(fd, event_buffer, sizeof(event_buffer));
        if(res < 0) {
            if(errno != EWOULDBLOCK) {
                perror("read inotify fd");
                throw std::runtime_error("failed to read inotify file descriptor");
            }
            return false;
        }
        auto* event = (struct inotify_event*)event_buffer;
        std::cout << "change for " << event->name << " " << event->mask << "\n";
        return filename == event->name;
    }

    ~inotify_watcher() override {
        inotify_rm_watch(fd, wd);
        close(fd);
    }
};
#endif

std::filesystem::path copy_path(const std::filesystem::path& lib_path, uint64_t counter) {
    return (
        lib_path.parent_path()
        / std::filesystem::path(
            path_to_string(lib_path.stem()) + std::to_string(counter)
            + path_to_string(lib_path.extension())
        )
    );
}

void* copy_and_load(const std::filesystem::path& lib_path, uint64_t counter) {
    auto c = copy_path(lib_path, counter);
    if(std::filesystem::exists(c)) std::filesystem::remove(c);
    std::cout << "copying " << lib_path << " to " << c << "\n";
    std::filesystem::copy_file(lib_path, c);
    return open_shared_library(c);
}

void unload_and_delete_copy(void* lib, const std::filesystem::path& lib_path, uint64_t counter) {
    close_shared_library(lib);
    auto c = copy_path(lib_path, counter);
    std::filesystem::remove(c);
}

shared_library_reloader::shared_library_reloader(std::filesystem::path library_path)
    : library_path(std::move(library_path)), reload_counter(0), current_lib(nullptr),
      watcher(nullptr) {}

shared_library_reloader::~shared_library_reloader() {
    if(current_lib != nullptr) unload_and_delete_copy(current_lib, library_path, reload_counter);
    delete watcher;
}

void* shared_library_reloader::initial_load() {
    std::cout << "loading and watching " << library_path << "\n";
    current_lib = copy_and_load(library_path, reload_counter);
#ifdef _MSC_VER
#elif __APPLE__
#else
    watcher = new inotify_watcher(library_path);
#endif
    return current_lib;
}

void shared_library_reloader::poll(const std::function<void(void*)>& on_reload) {
    if(watcher != nullptr && watcher->poll()) {
        // make sure the linker finishes writing the file
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "reloading " << library_path << "\n";
        void* new_lib = copy_and_load(library_path, reload_counter + 1);
        on_reload(new_lib);
        unload_and_delete_copy(current_lib, library_path, reload_counter);
        reload_counter++;
        current_lib = new_lib;
    }
}
