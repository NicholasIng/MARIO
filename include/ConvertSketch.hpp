#pragma once

#include <string>
#include <vector>
#include <glm/vec2.hpp>
#include "Util/Color.hpp"
#include "MapManager.hpp"
#include "Mario.hpp"
#include "Enemy.hpp"

bool convert_sketch(
    const std::string& path,
    MapManager& map,
    Mario& mario,
    Util::Color& background_color,
    std::vector<EnemySpawnInfo>* enemy_spawns = nullptr
);
