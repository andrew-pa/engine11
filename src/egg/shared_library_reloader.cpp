#include "egg/shared_library_reloader.h"
#include "FileWatch.hpp"

std::filesystem::path copy_path(const std::filesystem::path& lib_path, uint64_t counter) {
	return std::filesystem::path(path_to_string(lib_path.stem()) + std::to_string(counter) + path_to_string(lib_path.extension()));
}

void* copy_and_load(const std::filesystem::path& lib_path, uint64_t counter) {
	auto c = copy_path(lib_path, counter);
	if (std::filesystem::exists(c)) {
		std::filesystem::remove(c);
	}
	std::filesystem::copy_file(lib_path, c);
	return open_shared_library(c);
}

void unload_and_delete_copy(void* lib, const std::filesystem::path& lib_path, uint64_t counter) {
	close_shared_library(lib);
	auto c = copy_path(lib_path, counter);
	std::filesystem::remove(c);
}

shared_library_reloader::shared_library_reloader(std::filesystem::path p)
	: library_path(p), reload_counter(0), current_lib(nullptr), watcher(nullptr) {}

shared_library_reloader::~shared_library_reloader() {
	if (current_lib != nullptr)
		unload_and_delete_copy(current_lib, library_path, reload_counter);
	if (watcher != nullptr)
		delete (filewatch::FileWatch<std::string>*)watcher;
}

void* shared_library_reloader::initial_load() {
	std::cout << "loading and watching " << library_path << "\n";
	current_lib = copy_and_load(library_path, reload_counter);
	watcher = (void*)new filewatch::FileWatch<std::string>{
		path_to_string(library_path.parent_path()),
		[this](const std::string& p, filewatch::Event e) {
			if (e == filewatch::Event::added || e == filewatch::Event::modified || e == filewatch::Event::renamed_new) {
				if (true || p == library_path.filename()) {
					should_reload.store(true);
				}
			}
		}
	};
	return current_lib;
}

void shared_library_reloader::poll(std::function<void(void*)> on_reload) {
	if (should_reload.exchange(false)) {
		_sleep(1000);
		std::cout << "reloading " << library_path << "\n";
		void* new_lib = copy_and_load(library_path, reload_counter + 1);
		on_reload(new_lib);
		unload_and_delete_copy(current_lib, library_path, reload_counter);
		reload_counter++;
		current_lib = new_lib;
	}
}
