#pragma once

#include <filesystem>
#include <string>

#include "config.hpp"

namespace AssetPaths {

inline std::filesystem::path ResourceRoot() {
#ifdef RESOURCE_DIR
    return std::filesystem::path(RESOURCE_DIR);
#else
    return std::filesystem::path("Resources");
#endif
}

inline std::string Image(const std::string& relativePath) {
    return (ResourceRoot() / "image" / std::filesystem::path(relativePath)).string();
}

inline std::string Sound(const std::string& relativePath) {
    return (ResourceRoot() / "image" / "Sound" / std::filesystem::path(relativePath)).string();
}

} // namespace AssetPaths
