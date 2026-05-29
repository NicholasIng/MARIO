#pragma once

#include "AssetPaths.hpp"
#include "MapManager.hpp"
#include "config.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

extern std::unique_ptr<MapManager> g_MapManager;

namespace AppDetail {

constexpr float SKY_BLUE_R = 90.0f;
constexpr float SKY_BLUE_G = 147.0f;
constexpr float SKY_BLUE_B = 235.0f;
constexpr float CASTLE_TARGET_HEIGHT_TILES = 6.0f;
constexpr float CASTLE_OFFSET_TILES = 7.0f;
constexpr float CASTLE_END_INSET_TILES = 1.0f;
constexpr float GOAL_FINISH_DELAY = 1.2f;
constexpr float POST_GOAL_INTRO_DURATION = 2.0f;
constexpr float TRANSITION_PIPE_SINK_DURATION = 0.8f;
constexpr float TRANSITION_PIPE_SINK_SPEED = 96.0f;
constexpr float TRANSITION_PIPE_ENTRY_RANGE_TILES = 0.6f;
constexpr float TRANSITION_BLACKOUT_DURATION = 0.8f;
constexpr float TIME_BONUS_TICK_DURATION = 0.0075f;
constexpr float ENEMY_SPAWN_RANGE_X = WINDOW_WIDTH * 0.75f;
constexpr float LEVEL_INTRO_DURATION = 1.75f;
constexpr float FONT_GLYPH_SIZE = 8.0f;
constexpr int STARTING_TIMER = 400;
constexpr int STARTING_LIVES = 3;
constexpr float HUD_TEXT_SCALE = 3.0f;
constexpr float TITLE_TEXT_SCALE = 3.0f;
constexpr float INTRO_TEXT_SCALE = 3.0f;
constexpr float STATUS_MESSAGE_SCALE = 4.0f;
constexpr float COIN_FRAME_DURATION = 0.09f;
constexpr float TITLE_CURSOR_BLINK_DURATION = 0.18f;
constexpr float STATUS_MESSAGE_DURATION = 2.0f;
constexpr float TITLE_BG_R = 164.0f;
constexpr float TITLE_BG_G = 160.0f;
constexpr float TITLE_BG_B = 252.0f;
constexpr float UI_Z = 50.0f;
constexpr float TITLE_BG_Z = 24.0f;
constexpr float TITLE_DECOR_Z = 30.0f;
constexpr float TITLE_GROUND_Z = 20.0f;
constexpr float TITLE_LOGO_IMAGE_SCALE = 3.6f;
constexpr float TITLE_COPYRIGHT_SCALE = 2.9f;

inline std::string PadNumber(int value, int width) {
    std::ostringstream stream;
    stream << std::setw(width) << std::setfill('0') << std::max(0, value);
    return stream.str();
}

inline std::string WorldLabel(int world, int level) {
    return std::to_string(world) + "-" + std::to_string(level);
}

inline int DisplayLevelTime(float secondsRemaining) {
    return static_cast<int>(std::floor(std::max(0.0f, secondsRemaining)));
}

inline float CenteredTextX(const std::string& text, float scale, float spacing) {
    if (text.empty()) {
        return 0.0f;
    }

    const float advanceX = FONT_GLYPH_SIZE * scale + spacing;
    const float totalWidth = FONT_GLYPH_SIZE * scale + advanceX * static_cast<float>(text.size() - 1);
    return -totalWidth * 0.5f;
}

inline float GetLeftEdgeViewX(const MapManager* map) {
    if (map == nullptr) {
        return 0.0f;
    }

    const float halfScreen = WINDOW_WIDTH / 2.0f;
    const float minViewX = map->GetWorldLeft() + halfScreen;
    const float maxViewX = map->GetWorldRight() - halfScreen;
    if (minViewX > maxViewX) {
        return 0.0f;
    }
    return minViewX;
}

inline std::vector<std::string> CoinFramePaths() {
    return {
        AssetPaths::Image("homescreen/coins1.png"),
        AssetPaths::Image("homescreen/coins2.png"),
        AssetPaths::Image("homescreen/coins3.png"),
        AssetPaths::Image("homescreen/coins4.png"),
        AssetPaths::Image("homescreen/coins5.png"),
        AssetPaths::Image("homescreen/coins6.png"),
        AssetPaths::Image("homescreen/coins7.png"),
    };
}

} // namespace AppDetail
