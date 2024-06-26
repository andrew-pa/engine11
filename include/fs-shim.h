#pragma once
#include <filesystem>
#include <string>

#ifdef _MSC_VER
inline std::string path_to_string(const std::filesystem::path& p) { return p.generic_string(); }
#else
inline std::string path_to_string(const std::filesystem::path& p) { return p; }
#endif
