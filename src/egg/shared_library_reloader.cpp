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
    int            fd, wd;
    size_t         event_buffer_size;
    inotify_event* event_buffer;

    inotify_watcher(const std::filesystem::path& watched_file) {
        fd = inotify_init1(IN_NONBLOCK);
        if(fd < 0) {
            perror("create inotify fd");
            throw std::runtime_error("failed to create inotify file descriptor");
        }
        wd = inotify_add_watch(fd, watched_file.c_str(), IN_CLOSE_WRITE | IN_MODIFY);
        if(wd < 0) {
            perror("create inotify wd");
            throw std::runtime_error(
                std::string("failed to create inotify watch descriptor for ")
                + watched_file.string()
            );
        }
        event_buffer_size = sizeof(inotify_event) + NAME_MAX + 1;
        event_buffer      = (inotify_event*)malloc(event_buffer_size);
    }

    bool poll() override {
        int res = read(fd, event_buffer, event_buffer_size);
        if(res < 0) {
            if(errno != EWOULDBLOCK) {
                perror("read inotify fd");
                throw std::runtime_error("failed to read inotify file descriptor");
            }
            return false;
        }
        std::cout << "change for " << event_buffer->name << "\n";
        return true;
    }

    ~inotify_watcher() override {
        inotify_rm_watch(fd, wd);
        close(fd);
        free(event_buffer);
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
    : library_path(library_path), reload_counter(0), current_lib(nullptr), watcher(nullptr) {}

shared_library_reloader::~shared_library_reloader() {
    if(current_lib != nullptr) unload_and_delete_copy(current_lib, library_path, reload_counter);
    delete watcher;
    // delete (filewatch::FileWatch<std::string>*)watcher;
}

void* shared_library_reloader::initial_load() {
    std::cout << "loading and watching " << library_path << "\n";
    current_lib = copy_and_load(library_path, reload_counter);
    /*watcher = (void*)new filewatch::FileWatch<std::string>{
        path_to_string(library_path.parent_path()),
        [this](const std::string& p, filewatch::Event e) {
            if (e == filewatch::Event::added || e == filewatch::Event::modified || e ==
    filewatch::Event::renamed_new) { if (true || p == library_path.filename()) {
                    should_reload.store(true);
                }
            }
        }
    };*/
#ifdef _MSC_VER
#elif __APPLE__
#else
    watcher = new inotify_watcher(library_path);
#endif
    return current_lib;
}

void shared_library_reloader::poll(std::function<void(void*)> on_reload) {
    if(watcher != nullptr && watcher->poll()) {  //(should_reload.exchange(false)) {
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
