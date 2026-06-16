#pragma once

#include "Enemy.hpp"
#include "GameImage.hpp"
#include "MapManager.hpp"

#include <SDL.h>
#include <glm/vec2.hpp>

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ConvertSketchDetail {

using MaskGrid = std::vector<std::vector<char>>;

struct ComponentBounds {
    char kind = 0;
    int minX = 0;
    int maxX = 0;
    int minY = 0;
    int maxY = 0;
    std::vector<std::pair<int, int>> cells;
};

constexpr Uint8 SKY_BLUE_R = 90;
constexpr Uint8 SKY_BLUE_G = 147;
constexpr Uint8 SKY_BLUE_B = 235;

uint32_t PackRGB(Uint8 r, Uint8 g, Uint8 b);
bool GetPixelRGBA(SDL_Surface* surface, int x, int y, Uint8& r, Uint8& g, Uint8& b, Uint8& a);
int ColorDistanceSq(Uint8 r1, Uint8 g1, Uint8 b1, Uint8 r2, Uint8 g2, Uint8 b2);
std::string ToLower(std::string s);
bool IsSkyColor(Uint8 r, Uint8 g, Uint8 b);
bool FindTopRightSkyColor(SDL_Surface* surface, Uint8& outR, Uint8& outG, Uint8& outB);
bool IsMountainColor(Uint8 r, Uint8 g, Uint8 b);
bool IsBushColor(Uint8 r, Uint8 g, Uint8 b);
bool IsFlagColor(Uint8 r, Uint8 g, Uint8 b);
bool IsCastleColor(Uint8 r, Uint8 g, Uint8 b);
bool IsPipeForkedColor(Uint8 r, Uint8 g, Uint8 b);
bool IsMovingPlatformColor(Uint8 r, Uint8 g, Uint8 b);
bool IsHorizontalMovingPlatformColor(Uint8 r, Uint8 g, Uint8 b);
bool IsWoodColor(Uint8 r, Uint8 g, Uint8 b);
bool IsTreeColor(Uint8 r, Uint8 g, Uint8 b);
bool FindMarioSpawnMarker(SDL_Surface* surface,
                          int layerHeight,
                          int& outGridX,
                          int& outEntityGridY,
                          bool& outFoundOutsideEntityLayer);
glm::vec2 ComputeGroundedSpawnPosition(const MapManager& map,
                                       int gridX,
                                       int entityGridY,
                                       float halfHeight);
glm::vec2 ComputeMarkerSpawnPosition(const MapManager& map,
                                     int gridX,
                                     int entityGridY);

template <typename TValue>
const TValue* FindClosestColorMatch(const std::unordered_map<uint32_t, TValue>& entries,
                                    Uint8 r,
                                    Uint8 g,
                                    Uint8 b,
                                    int maxDistanceSq) {
    const TValue* best = nullptr;
    int bestDistance = std::numeric_limits<int>::max();

    for (const auto& [packed, value] : entries) {
        const Uint8 mr = static_cast<Uint8>((packed >> 16) & 0xFF);
        const Uint8 mg = static_cast<Uint8>((packed >> 8) & 0xFF);
        const Uint8 mb = static_cast<Uint8>(packed & 0xFF);
        const int distance = ColorDistanceSq(r, g, b, mr, mg, mb);
        if (distance <= maxDistanceSq && distance < bestDistance) {
            bestDistance = distance;
            best = &value;
        }
    }

    return best;
}

std::vector<ComponentBounds> CollectComponents(const MaskGrid& mask,
                                              int width,
                                              int height);
std::vector<ComponentBounds> CollectComponentsOfKind(const MaskGrid& mask,
                                                    int width,
                                                    int height,
                                                    char kind);
void AddComponentSprites(MapManager& map,
                         const MaskGrid& mask,
                         int width,
                         int layerHeight,
                         bool isBackground,
                         Cell type,
                         const std::string& resolvedPath,
                         const std::function<void(int, int, int, int)>& onComponent = {});
void AddMarkerScaledSprite(MapManager& map,
                           const MaskGrid& mask,
                           int width,
                           int layerHeight,
                           const std::string& resolvedPath,
                           bool drawInForeground = false,
                           const std::function<void(int, int, int, int, int, int)>& onComponent = {});
void ProcessMovingPlatforms(MapManager& map,
                            const MaskGrid& movingPlatformMask,
                            int width,
                            int layerHeight,
                            bool isUndergroundLevel);
void ProcessFlags(MapManager& map,
                  const MaskGrid& flagMask,
                  int width,
                  int layerHeight,
                  bool isUndergroundLevel);
void AppendEnemySpawns(const MapManager& map,
                       const MaskGrid& mask,
                       int width,
                       int layerHeight,
                       EnemyKind kind,
                       std::vector<EnemySpawnInfo>& enemySpawns);
void AddVenusSpawns(const std::vector<glm::vec2>& venusSpawnPositions,
                    std::vector<EnemySpawnInfo>& enemySpawns);
void TweakWingedRedKoopaSpawns(std::vector<EnemySpawnInfo>& enemySpawns);
void ApplyHardBlockReplacement(MapManager& map,
                               int width,
                               int layerHeight,
                               bool isUndergroundLevel);

}  // namespace ConvertSketchDetail
