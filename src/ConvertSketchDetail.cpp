#include "ConvertSketchDetail.hpp"

#include "Util/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <queue>

namespace fs = std::filesystem;

namespace ConvertSketchDetail {

uint32_t PackRGB(Uint8 r, Uint8 g, Uint8 b) {
    return (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
}

bool GetPixelRGBA(SDL_Surface* surface, int x, int y, Uint8& r, Uint8& g, Uint8& b, Uint8& a) {
    if (!surface || x < 0 || y < 0 || x >= surface->w || y >= surface->h) {
        return false;
    }
    Uint32* pixels = static_cast<Uint32*>(surface->pixels);
    const int pitchPixels = surface->pitch / sizeof(Uint32);
    const Uint32 px = pixels[y * pitchPixels + x];
    SDL_GetRGBA(px, surface->format, &r, &g, &b, &a);
    return true;
}

int ColorDistanceSq(Uint8 r1, Uint8 g1, Uint8 b1, Uint8 r2, Uint8 g2, Uint8 b2) {
    const int dr = static_cast<int>(r1) - static_cast<int>(r2);
    const int dg = static_cast<int>(g1) - static_cast<int>(g2);
    const int db = static_cast<int>(b1) - static_cast<int>(b2);
    return dr * dr + dg * dg + db * db;
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool IsSkyColor(Uint8 r, Uint8 g, Uint8 b) {
    return (b > 140) && (b > r + 30) && (b > g + 10);
}

bool FindTopRightSkyColor(SDL_Surface* surface, Uint8& outR, Uint8& outG, Uint8& outB) {
    if (!surface) {
        return false;
    }
    const int searchW = std::min(32, surface->w);
    const int searchH = std::min(32, surface->h);
    for (int yy = 0; yy < searchH; ++yy) {
        for (int xx = surface->w - 1; xx >= surface->w - searchW; --xx) {
            Uint8 r, g, b, a;
            if (!GetPixelRGBA(surface, xx, yy, r, g, b, a) || a == 0) {
                continue;
            }
            if (IsSkyColor(r, g, b)) {
                outR = r;
                outG = g;
                outB = b;
                return true;
            }
        }
    }
    return false;
}

bool IsMountainColor(Uint8 r, Uint8 g, Uint8 b) {
    const uint32_t key = PackRGB(r, g, b);
    return key == PackRGB(0, 73, 0) || key == PackRGB(0, 109, 0);
}

bool IsBushColor(Uint8 r, Uint8 g, Uint8 b) {
    const uint32_t key = PackRGB(r, g, b);
    return key == PackRGB(146, 219, 0) ||
           key == PackRGB(146, 182, 0) ||
           key == PackRGB(146, 146, 0);
}

bool IsFlagColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(109, 255, 85);
}

bool IsCastleColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(255, 216, 0);
}

bool IsPipeForkedColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(0, 255, 176);
}

bool IsMovingPlatformColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(255, 205, 0);
}

bool IsHorizontalMovingPlatformColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(148, 119, 0);
}

bool IsWoodColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(255, 115, 0);
}

bool IsTreeColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(71, 255, 40);
}

bool FindMarioSpawnMarker(SDL_Surface* surface,
                          int layerHeight,
                          int& outGridX,
                          int& outEntityGridY,
                          bool& outFoundOutsideEntityLayer) {
    if (!surface || layerHeight <= 0) {
        return false;
    }

    outFoundOutsideEntityLayer = false;

    for (int x = 0; x < surface->w; ++x) {
        for (int y = 0; y < layerHeight; ++y) {
            Uint8 r, g, b, a;
            if (!GetPixelRGBA(surface, x, y + layerHeight, r, g, b, a)) {
                continue;
            }
            if (a > 0 && r == 255 && g == 0 && b == 0) {
                outGridX = x;
                outEntityGridY = y;
                return true;
            }
        }
    }

    for (int x = 0; x < surface->w; ++x) {
        for (int y = 0; y < surface->h; ++y) {
            Uint8 r, g, b, a;
            if (!GetPixelRGBA(surface, x, y, r, g, b, a)) {
                continue;
            }
            if (a > 0 && r == 255 && g == 0 && b == 0) {
                outGridX = x;
                outEntityGridY = y % layerHeight;
                outFoundOutsideEntityLayer = (y < layerHeight || y >= 2 * layerHeight);
                return true;
            }
        }
    }

    return false;
}

glm::vec2 ComputeGroundedSpawnPosition(const MapManager& map,
                                       int gridX,
                                       int entityGridY,
                                       float halfHeight) {
    const float tileSize = map.GetTileSize();
    const int clampedGridX = std::clamp(gridX, 0, std::max(0, map.GetWidth() - 1));
    const float worldX = map.GetWorldLeft() + clampedGridX * tileSize + tileSize / 2.0f;
    const int minSearchX = std::max(0, clampedGridX - 1);
    const int maxSearchX = std::min(std::max(0, map.GetWidth() - 1), clampedGridX + 1);

    for (int y = std::max(0, entityGridY); y < map.GetHeight(); ++y) {
        for (int supportX = minSearchX; supportX <= maxSearchX; ++supportX) {
            if (MapManager::IsSolidCell(map.GetCell(supportX, y))) {
                const float tileTop = (map.GetHeight() * tileSize) / 2.0f - y * tileSize;
                return {worldX, tileTop + halfHeight};
            }
        }
    }

    const float fallbackY =
        (map.GetHeight() * tileSize) / 2.0f - entityGridY * tileSize + tileSize / 2.0f;
    return {worldX, fallbackY};
}

glm::vec2 ComputeMarkerSpawnPosition(const MapManager& map, int gridX, int entityGridY) {
    const float tileSize = map.GetTileSize();
    const int clampedGridX = std::clamp(gridX, 0, std::max(0, map.GetWidth() - 1));
    const int clampedGridY = std::clamp(entityGridY, 0, std::max(0, map.GetHeight() - 1));
    const float worldX = map.GetWorldLeft() + clampedGridX * tileSize + tileSize / 2.0f;
    const float worldY =
        (map.GetHeight() * tileSize) / 2.0f - clampedGridY * tileSize - tileSize / 2.0f;
    return {worldX, worldY};
}

namespace {

std::vector<ComponentBounds> CollectComponentsImpl(const MaskGrid& mask,
                                                  int width,
                                                  int height,
                                                  bool matchSpecificKind,
                                                  char requiredKind) {
    std::vector<ComponentBounds> components;
    std::vector<std::vector<char>> visited(width, std::vector<char>(height, 0));

    for (int sx = 0; sx < width; ++sx) {
        for (int sy = 0; sy < height; ++sy) {
            const char cell = mask[sx][sy];
            if (visited[sx][sy] || cell == 0) {
                continue;
            }
            if (matchSpecificKind && cell != requiredKind) {
                continue;
            }

            ComponentBounds component;
            component.kind = cell;
            component.minX = sx;
            component.maxX = sx;
            component.minY = sy;
            component.maxY = sy;

            std::queue<std::pair<int, int>> q;
            q.push({sx, sy});
            visited[sx][sy] = 1;

            while (!q.empty()) {
                const auto [cx, cy] = q.front();
                q.pop();
                component.cells.push_back({cx, cy});
                component.minX = std::min(component.minX, cx);
                component.maxX = std::max(component.maxX, cx);
                component.minY = std::min(component.minY, cy);
                component.maxY = std::max(component.maxY, cy);

                const int dx[4] = {1, -1, 0, 0};
                const int dy[4] = {0, 0, 1, -1};
                for (int i = 0; i < 4; ++i) {
                    const int nx = cx + dx[i];
                    const int ny = cy + dy[i];
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                        continue;
                    }
                    if (visited[nx][ny] || mask[nx][ny] == 0) {
                        continue;
                    }
                    if (matchSpecificKind && mask[nx][ny] != requiredKind) {
                        continue;
                    }
                    if (!matchSpecificKind && mask[nx][ny] != cell) {
                        continue;
                    }
                    visited[nx][ny] = 1;
                    q.push({nx, ny});
                }
            }

            components.push_back(std::move(component));
        }
    }

    return components;
}

std::pair<int, int> GetMarkerSpriteTileSize(MapManager& map, const std::string& resolvedPath) {
    GameImage image(resolvedPath);
    const glm::vec2 imageSize = image.GetSize();
    return {
        std::max(1, static_cast<int>(std::round((imageSize.x * 3.0f) / map.GetTileSize()))),
        std::max(1, static_cast<int>(std::round((imageSize.y * 3.0f) / map.GetTileSize())))
    };
}

}  // namespace

std::vector<ComponentBounds> CollectComponents(const MaskGrid& mask, int width, int height) {
    return CollectComponentsImpl(mask, width, height, false, 0);
}

std::vector<ComponentBounds> CollectComponentsOfKind(const MaskGrid& mask,
                                                    int width,
                                                    int height,
                                                    char kind) {
    return CollectComponentsImpl(mask, width, height, true, kind);
}

void AddComponentSprites(MapManager& map,
                         const MaskGrid& mask,
                         int width,
                         int layerHeight,
                         bool isBackground,
                         Cell type,
                         const std::string& resolvedPath,
                         const std::function<void(int, int, int, int)>& onComponent) {
    for (const auto& component : CollectComponents(mask, width, layerHeight)) {
        const int spanX = component.maxX - component.minX + 1;
        const int spanY = component.maxY - component.minY + 1;
        if (isBackground) {
            map.AddBackgroundSprite(component.minX, component.minY, spanX, spanY, resolvedPath);
        } else {
            map.AddTileSprite(component.minX, component.minY, spanX, spanY, type, resolvedPath);
        }
        if (onComponent) {
            onComponent(component.minX, component.minY, spanX, spanY);
        }
    }
}

void AddMarkerScaledSprite(MapManager& map,
                           const MaskGrid& mask,
                           int width,
                           int layerHeight,
                           const std::string& resolvedPath,
                           bool drawInForeground,
                           const std::function<void(int, int, int, int, int, int)>& onComponent) {
    if (resolvedPath.empty() || !fs::exists(resolvedPath)) {
        return;
    }

    GameImage image(resolvedPath);
    const glm::vec2 imageSize = image.GetSize();
    if (imageSize.x <= 0.0f || imageSize.y <= 0.0f) {
        return;
    }

    const int spriteTileWidth =
        std::max(1, static_cast<int>(std::round((imageSize.x * 3.0f) / map.GetTileSize())));
    const int spriteTileHeight =
        std::max(1, static_cast<int>(std::round((imageSize.y * 3.0f) / map.GetTileSize())));

    for (const auto& component : CollectComponents(mask, width, layerHeight)) {
        const int markerCenterX = (component.minX + component.maxX) / 2;
        const int markerBaseY = component.maxY;
        const int startX = std::clamp(
            markerCenterX - spriteTileWidth / 2,
            0,
            std::max(0, width - spriteTileWidth));
        const int startY = std::clamp(
            markerBaseY - spriteTileHeight + 1,
            0,
            std::max(0, layerHeight - spriteTileHeight));

        if (drawInForeground) {
            map.AddForegroundSprite(startX, startY, spriteTileWidth, spriteTileHeight, resolvedPath);
        } else {
            map.AddBackgroundSprite(startX, startY, spriteTileWidth, spriteTileHeight, resolvedPath);
        }
        if (onComponent) {
            onComponent(startX, startY, spriteTileWidth, spriteTileHeight, markerCenterX, markerBaseY);
        }
    }
}

void ProcessMovingPlatforms(MapManager& map,
                            const MaskGrid& movingPlatformMask,
                            int width,
                            int layerHeight,
                            bool isUndergroundLevel) {
    auto markers = CollectComponents(movingPlatformMask, width, layerHeight);
    std::vector<char> used(markers.size(), 0);
    const int horizontalPlatformCount = static_cast<int>(std::count_if(
        markers.begin(), markers.end(), [](const ComponentBounds& marker) {
            return marker.kind == 2;
        }));
    int horizontalPlatformIndex = 0;
    std::vector<std::size_t> verticalPlatformOrder;
    for (std::size_t i = 0; i < markers.size(); ++i) {
        if (markers[i].kind != 2) {
            verticalPlatformOrder.push_back(i);
        }
    }
    std::sort(verticalPlatformOrder.begin(), verticalPlatformOrder.end(),
              [&](std::size_t left, std::size_t right) {
                  if (markers[left].minX != markers[right].minX) {
                      return markers[left].minX < markers[right].minX;
                  }
                  return markers[left].minY < markers[right].minY;
              });

    for (std::size_t i = 0; i < markers.size(); ++i) {
        if (used[i]) {
            continue;
        }

        const ComponentBounds& marker = markers[i];
        const int spanX = marker.maxX - marker.minX + 1;
        const int spanY = marker.maxY - marker.minY + 1;

        if (marker.kind == 2) {
            const bool isFinalHorizontalPlatform =
                horizontalPlatformIndex == horizontalPlatformCount - 1;
            const bool shouldStaggerStart =
                !isFinalHorizontalPlatform && horizontalPlatformIndex == 1;
            const int leftGridX = marker.minX - 4;
            const int rightGridX = isFinalHorizontalPlatform ? marker.minX + 4 : marker.minX;
            map.AddMovingPlatform(marker.minX,
                                  marker.minY,
                                  marker.maxY,
                                  spanX,
                                  "platform.png",
                                  72.0f,
                                  0.0f,
                                  MapManager::MovingPlatformMotion::Horizontal,
                                  4,
                                  leftGridX,
                                  rightGridX,
                                  true,
                                  shouldStaggerStart ? 1.35f : 0.0f);
            ++horizontalPlatformIndex;
        } else {
            const int topGridY = marker.minY;
            const int bottomGridY = (spanY > 1) ? marker.maxY : layerHeight - 1;
            const auto orderIt = std::find(verticalPlatformOrder.begin(), verticalPlatformOrder.end(), i);
            const int verticalPlatformIndex = orderIt == verticalPlatformOrder.end()
                ? 0
                : static_cast<int>(std::distance(verticalPlatformOrder.begin(), orderIt));
            const int verticalPlatformHalf =
                std::max(1, static_cast<int>((verticalPlatformOrder.size() + 1) / 2));
            const bool isWrappedUndergroundPlatform = isUndergroundLevel;
            const bool isDownCycle =
                isWrappedUndergroundPlatform && verticalPlatformIndex < verticalPlatformHalf;
            const int verticalPlatformGroupSize = isWrappedUndergroundPlatform
                ? (isDownCycle
                    ? verticalPlatformHalf
                    : static_cast<int>(verticalPlatformOrder.size()) - verticalPlatformHalf)
                : 1;
            const int verticalPlatformGroupIndex = isWrappedUndergroundPlatform
                ? (isDownCycle
                    ? verticalPlatformIndex
                    : verticalPlatformIndex - verticalPlatformHalf)
                : 0;
            if (isWrappedUndergroundPlatform &&
                verticalPlatformGroupSize >= 2 &&
                verticalPlatformGroupIndex >= 2) {
                used[i] = 1;
                continue;
            }

            const MapManager::MovingPlatformCycle platformCycle =
                isWrappedUndergroundPlatform
                    ? (isDownCycle
                        ? MapManager::MovingPlatformCycle::WrapDown
                        : MapManager::MovingPlatformCycle::WrapUp)
                    : MapManager::MovingPlatformCycle::Bounce;
            const int platformCopies =
                isWrappedUndergroundPlatform && verticalPlatformGroupSize == 1 ? 2 : 1;
            for (int copy = 0; copy < platformCopies; ++copy) {
                const float cycleOffset = platformCopies > 1
                    ? static_cast<float>(copy) / static_cast<float>(platformCopies)
                    : 0.0f;
                map.AddMovingPlatform(marker.minX,
                                      topGridY,
                                      bottomGridY,
                                      spanX,
                                      "platform.png",
                                      72.0f,
                                      platformCycle == MapManager::MovingPlatformCycle::Bounce ? 0.18f : 0.0f,
                                      MapManager::MovingPlatformMotion::Vertical,
                                      6,
                                      -1,
                                      -1,
                                      false,
                                      0.0f,
                                      platformCycle,
                                      cycleOffset);
            }
        }
        used[i] = 1;
    }
}

void ProcessFlags(MapManager& map,
                  const MaskGrid& flagMask,
                  int width,
                  int layerHeight,
                  bool isUndergroundLevel) {
    for (const auto& flag : CollectComponents(flagMask, width, layerHeight)) {
        const std::string flagstickPath = MapManager::ResolveBackgroundPath("flagstick.png");
        const std::string dotPath = MapManager::ResolveBackgroundPath("dot.png");
        const std::string flagPath = MapManager::ResolveBackgroundPath("flag.png");
        const std::string hardBlockPath = MapManager::ResolveTilePath(
            Cell::Wall,
            isUndergroundLevel ? "SMB_Underground_Hard_Block.png" : "HardBlock.png");
        const int poleBottomY = flag.maxY;
        const int maxBaseY = layerHeight - 1;
        const int baseY = std::min(maxBaseY, poleBottomY + 1);
        const int flagX = std::max(0, flag.minX - 1);
        const int flagY = std::min(poleBottomY, flag.minY + 1);
        const int clearTopY = std::max(0, flag.minY - 3);

        for (int clearX = flagX; clearX <= flag.maxX; ++clearX) {
            for (int clearY = clearTopY; clearY <= baseY; ++clearY) {
                map.ClearTile(clearX, clearY);
            }
        }

        for (int poleY = flag.minY + 1; poleY <= poleBottomY; ++poleY) {
            map.AddBackgroundTile(flag.minX, poleY, flagstickPath);
        }
        map.AddBackgroundTile(flag.minX, flag.minY, dotPath);
        map.AddTile(flag.minX, baseY, Cell::Wall, hardBlockPath);
        map.ConfigureGoalVisuals(flag.minX, flagX, flagY, poleBottomY, baseY, flagPath);
    }
}

void AppendEnemySpawns(const MapManager& map,
                       const MaskGrid& mask,
                       int width,
                       int layerHeight,
                       EnemyKind kind,
                       std::vector<EnemySpawnInfo>& enemySpawns) {
    const float halfHeight =
        (kind == EnemyKind::GreenKoopa ||
         kind == EnemyKind::RedKoopa ||
         kind == EnemyKind::RedKoopaWinged) ? 36.0f : 24.0f;

    for (const auto& component : CollectComponents(mask, width, layerHeight)) {
        const int centerX = (component.minX + component.maxX) / 2;
        if (kind == EnemyKind::RedKoopaWinged) {
            enemySpawns.push_back({ComputeMarkerSpawnPosition(map, centerX, component.minY), kind});
            continue;
        }

        enemySpawns.push_back({
            ComputeGroundedSpawnPosition(map, centerX, component.minY, halfHeight),
            kind
        });
    }
}

void AddVenusSpawns(const std::vector<glm::vec2>& venusSpawnPositions,
                    std::vector<EnemySpawnInfo>& enemySpawns) {
    if (venusSpawnPositions.empty()) {
        return;
    }

    std::vector<glm::vec2> sortedPositions = venusSpawnPositions;
    std::sort(sortedPositions.begin(), sortedPositions.end(),
              [](const glm::vec2& lhs, const glm::vec2& rhs) {
                  return lhs.x < rhs.x;
              });
    const std::size_t plantCount = std::min<std::size_t>(2, sortedPositions.size());
    for (std::size_t i = 0; i < plantCount; ++i) {
        enemySpawns.push_back({sortedPositions[i], EnemyKind::Venus});
    }
}

void TweakWingedRedKoopaSpawns(std::vector<EnemySpawnInfo>& enemySpawns) {
    bool firstWingedRedKoopa = true;
    for (auto& spawn : enemySpawns) {
        if (spawn.kind != EnemyKind::RedKoopaWinged) {
            continue;
        }
        if (firstWingedRedKoopa) {
            spawn.flightTopTiles = 5.0f;
            spawn.flightBottomTiles = 1.0f;
            firstWingedRedKoopa = false;
        }
    }
}

void ApplyHardBlockReplacement(MapManager& map,
                               int width,
                               int layerHeight,
                               bool isUndergroundLevel) {
    const std::string hardBlockPath = MapManager::ResolveTilePath(
        Cell::Brick,
        isUndergroundLevel ? "SMB_Underground_Hard_Block.png" : "HardBlock.png");
    const bool haveHardBlockAsset = !hardBlockPath.empty() && fs::exists(hardBlockPath);
    if (!haveHardBlockAsset || isUndergroundLevel) {
        return;
    }

    std::vector<std::vector<char>> visited(width, std::vector<char>(layerHeight, 0));
    for (int sx = 0; sx < width; ++sx) {
        for (int sy = 0; sy < layerHeight; ++sy) {
            if (visited[sx][sy] || map.GetCell(sx, sy) != Cell::Brick) {
                continue;
            }

            std::vector<std::pair<int, int>> comp;
            std::queue<std::pair<int, int>> q;
            q.push({sx, sy});
            visited[sx][sy] = 1;

            while (!q.empty()) {
                const auto [cx, cy] = q.front();
                q.pop();
                comp.push_back({cx, cy});

                const int dx[4] = {1, -1, 0, 0};
                const int dy[4] = {0, 0, 1, -1};
                for (int i = 0; i < 4; ++i) {
                    const int nx = cx + dx[i];
                    const int ny = cy + dy[i];
                    if (nx < 0 || nx >= width || ny < 0 || ny >= layerHeight) {
                        continue;
                    }
                    if (visited[nx][ny] || map.GetCell(nx, ny) != Cell::Brick) {
                        continue;
                    }
                    visited[nx][ny] = 1;
                    q.push({nx, ny});
                }
            }

            if (static_cast<int>(comp.size()) < 6) {
                continue;
            }

            int minX = width;
            int maxX = -1;
            for (const auto& p : comp) {
                minX = std::min(minX, p.first);
                maxX = std::max(maxX, p.first);
            }
            if (minX < static_cast<int>(width * 0.6f)) {
                continue;
            }

            const int cols = maxX - minX + 1;
            if (cols < 3) {
                continue;
            }

            std::vector<int> topY(cols, layerHeight + 1);
            std::vector<int> colCount(cols, 0);
            for (const auto& p : comp) {
                const int cx = p.first - minX;
                topY[cx] = std::min(topY[cx], p.second);
                colCount[cx]++;
            }

            std::vector<int> seq;
            for (int i = 0; i < cols; ++i) {
                if (colCount[i] > 0) {
                    seq.push_back(topY[i]);
                }
            }
            if (static_cast<int>(seq.size()) < 3) {
                continue;
            }

            int sign = 0;
            bool monotonic = true;
            for (std::size_t i = 1; i < seq.size(); ++i) {
                const int d = seq[i] - seq[i - 1];
                if (d == 0) {
                    continue;
                }
                const int s = (d > 0) ? 1 : -1;
                if (sign == 0) {
                    sign = s;
                } else if (sign != s) {
                    monotonic = false;
                    break;
                }
            }
            if (!monotonic) {
                continue;
            }

            for (const auto& p : comp) {
                map.AddTile(p.first, p.second, Cell::Brick, hardBlockPath);
            }
        }
    }
}

}  // namespace ConvertSketchDetail
