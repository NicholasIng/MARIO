#pragma once

#include <string>
#include "Util/Color.hpp"
#include "MapManager.hpp"
#include "Mario.hpp"

bool convert_sketch(
    const std::string& path,
    MapManager& map,
    Mario& mario,
    Util::Color& background_color
);