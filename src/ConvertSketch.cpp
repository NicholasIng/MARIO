#include "ConvertSketch.hpp"
#include "Util/Logger.hpp"
#include "MapManager.hpp"
#include <SDL_image.h>
#include <unordered_map>
#include <filesystem>
#include <cstdint>
#include <cctype>
#include <algorithm>
#include <vector>
#include <string>
#include <queue>
#include <limits>

namespace fs = std::filesystem;

static inline uint32_t PackRGB(Uint8 r, Uint8 g, Uint8 b) {
    return (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8)  |
           (static_cast<uint32_t>(b));
}

static bool GetPixelRGBA(SDL_Surface* surface, int x, int y, Uint8& r, Uint8& g, Uint8& b, Uint8& a) {
    if (!surface || x < 0 || y < 0 || x >= surface->w || y >= surface->h) return false;
    Uint32* pixels = static_cast<Uint32*>(surface->pixels);
    int pitchPixels = surface->pitch / sizeof(Uint32);
    Uint32 px = pixels[y * pitchPixels + x];
    SDL_GetRGBA(px, surface->format, &r, &g, &b, &a);
    return true;
}

static int ColorDistanceSq(Uint8 r1, Uint8 g1, Uint8 b1, Uint8 r2, Uint8 g2, Uint8 b2) {
    int dr = static_cast<int>(r1) - static_cast<int>(r2);
    int dg = static_cast<int>(g1) - static_cast<int>(g2);
    int db = static_cast<int>(b1) - static_cast<int>(b2);
    return dr * dr + dg * dg + db * db;
}

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

// find first file under dir whose filename contains any keyword (case-insensitive)
static std::string FindResourceByKeywords(const fs::path& dir, const std::vector<std::string>& keywords) {
    try {
        if (!fs::exists(dir) || !fs::is_directory(dir)) return std::string();
        for (auto &entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            std::string name = ToLower(entry.path().filename().string());
            for (auto &k : keywords) {
                if (name.find(ToLower(k)) != std::string::npos) {
                    return entry.path().string();
                }
            }
        }
    } catch (const std::exception &e) {
        LOG_WARN("FindResourceByKeywords exception: {}", e.what());
    }
    return std::string();
}

// quick heuristic to detect sky/blue background colors (blue-dominant)
static bool IsSkyColor(Uint8 r, Uint8 g, Uint8 b) {
    return (b > 140) && (b > r + 30) && (b > g + 10);
}

// Scan a small top-right region of the sketch for a blue/sky pixel and return it.
// This helps pick the tiny sky-blue marker in LevelSketch0 and use it as the scene background.
static bool FindTopRightSkyColor(SDL_Surface* surface, Uint8& outR, Uint8& outG, Uint8& outB) {
    if (!surface) return false;
    int searchW = std::min(32, surface->w);
    int searchH = std::min(32, surface->h);
    // search from top-right inward for first sky-like pixel
    for (int yy = 0; yy < searchH; ++yy) {
        for (int xx = surface->w - 1; xx >= surface->w - searchW; --xx) {
            Uint8 r,g,b,a;
            if (!GetPixelRGBA(surface, xx, yy, r, g, b, a)) continue;
            if (a == 0) continue;
            if (IsSkyColor(r,g,b)) {
                outR = r; outG = g; outB = b;
                return true;
            }
        }
    }
    return false;
}

template <typename TValue>
static const TValue* FindClosestColorMatch(
    const std::unordered_map<uint32_t, TValue>& entries,
    Uint8 r,
    Uint8 g,
    Uint8 b,
    int maxDistanceSq
) {
    const TValue* best = nullptr;
    int bestDistance = std::numeric_limits<int>::max();

    for (const auto& [packed, value] : entries) {
        Uint8 mr = static_cast<Uint8>((packed >> 16) & 0xFF);
        Uint8 mg = static_cast<Uint8>((packed >> 8) & 0xFF);
        Uint8 mb = static_cast<Uint8>(packed & 0xFF);
        int distance = ColorDistanceSq(r, g, b, mr, mg, mb);
        if (distance <= maxDistanceSq && distance < bestDistance) {
            bestDistance = distance;
            best = &value;
        }
    }

    return best;
}

static bool IsMountainColor(Uint8 r, Uint8 g, Uint8 b) {
    uint32_t key = PackRGB(r, g, b);
    return key == PackRGB(0, 73, 0) ||
           key == PackRGB(0, 109, 0);
}

static bool IsBushColor(Uint8 r, Uint8 g, Uint8 b) {
    uint32_t key = PackRGB(r, g, b);
    return key == PackRGB(146, 219, 0) ||
           key == PackRGB(146, 182, 0) ||
           key == PackRGB(146, 146, 0);
}

static bool IsFlagColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(109, 255, 85);
}

static glm::vec2 ComputeGroundedSpawnPosition(const MapManager& map,
                                              int gridX,
                                              int entityGridY,
                                              float halfHeight) {
    const float tileSize = map.GetTileSize();
    const float worldX = map.GetWorldLeft() + gridX * tileSize + tileSize / 2.0f;
    for (int y = std::max(0, entityGridY); y < map.GetHeight(); ++y) {
        if (MapManager::IsSolidCell(map.GetCell(gridX, y))) {
            const float tileTop = (map.GetHeight() * tileSize) / 2.0f - y * tileSize;
            return { worldX, tileTop + halfHeight };
        }
    }

    const float fallbackY = (map.GetHeight() * tileSize) / 2.0f - entityGridY * tileSize + tileSize / 2.0f;
    return { worldX, fallbackY };
}

bool convert_sketch(
    const std::string& path,
    MapManager& map,
    Mario& mario,
    Util::Color& background_color,
    std::vector<glm::vec2>* enemy_spawns
) {
    SDL_Surface* loaded = IMG_Load(path.c_str());
    if (!loaded) {
        LOG_ERROR("Failed to load sketch: {}", path);
        LOG_ERROR("IMG error: {}", IMG_GetError());
        return false;
    }

    SDL_Surface* surface = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loaded);

    if (!surface) {
        LOG_ERROR("Failed to convert sketch surface: {}", path);
        return false;
    }

    int width = surface->w;
    int totalHeight = surface->h;

    if (totalHeight % 3 != 0) {
        LOG_ERROR("Sketch height must be divisible by 3. Height = {}", totalHeight);
        SDL_FreeSurface(surface);
        return false;
    }

    int layerHeight = totalHeight / 3;
    map.SetMapSize(width, layerHeight);

    background_color = Util::Color(0, 219, 255, 255);

    // tile mappings: packed RGB -> pair(Cell type, requested filename)
    struct TileEntry { Cell type; std::string requested; };
    std::unordered_map<uint32_t, TileEntry> tileMap = {
        { PackRGB(0,   0,   0),   { Cell::Wall,          "Ground.png" } },
        { PackRGB(182, 73,  0),   { Cell::Brick,         "Brick.png" } },
        { PackRGB(146, 73,  0),   { Cell::Brick,         "Brick.png" } },
        { PackRGB(255, 255, 0),   { Cell::Coin,          "Coin.png" } },
        { PackRGB(255, 73,  85),  { Cell::QuestionBlock, "Question.png" } },
        { PackRGB(255, 146, 85),  { Cell::QuestionBlock, "Question.png" } },
        { PackRGB(0,   146, 0),   { Cell::Pipe,          "Pipe.png" } },
        { PackRGB(0,   182, 0),   { Cell::Pipe,          "Pipe.png" } },
        { PackRGB(0,   219, 0),   { Cell::Pipe,          "Pipe.png" } },
        // optional stair-colored pixels: if the resources contain a "Stair.png"
        { PackRGB(128, 128, 128), { Cell::Brick,         "Stair.png" } }, // common gray stair
        { PackRGB(150,  75,   0), { Cell::Brick,         "Stair.png" } }  // brown-ish stair
    };

    bool spawnFound = false;

    // pre-resolve stair fallback: if a "Stair.png" exists (under resources) use it for unmatched top-layer tiles
    std::string stairResolved = MapManager::ResolveTilePath(Cell::Brick, "Stair.png");
    bool haveStairAsset = false;
    if (!stairResolved.empty() && fs::exists(stairResolved)) {
        haveStairAsset = true;
        LOG_INFO("Using Stair asset: {}", stairResolved);
    }

    std::string pipeResolved = MapManager::ResolveTilePath(Cell::Pipe, "Pipe.png");
    std::string mountainResolved = MapManager::ResolveBackgroundPath("mountains.png");
    std::string bushResolved = MapManager::ResolveBackgroundPath("Bush.png");
    std::string cloudResolved = MapManager::ResolveBackgroundPath("Clouds_2.png");
    std::vector<std::vector<char>> pipeMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> mountainMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> bushMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> cloudMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> flagMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> goombaMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> isHard(width, std::vector<char>(layerHeight, 0));

    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < layerHeight; ++y) {
            Uint8 r,g,b,a;

            // =========================
            // TOP 1/3 = BLOCKS
            // =========================
            if (!GetPixelRGBA(surface, x, y, r, g, b, a)) continue;
            if (a > 0) {
                uint32_t key = PackRGB(r,g,b);
                auto it = tileMap.find(key);
                const TileEntry* matchedTile = nullptr;
                if (it != tileMap.end()) {
                    matchedTile = &it->second;
                } else {
                    matchedTile = FindClosestColorMatch(tileMap, r, g, b, 60 * 60);
                }

                if (matchedTile != nullptr) {
                    if (matchedTile->type == Cell::Pipe) {
                        pipeMask[x][y] = 1;
                    } else {
                        std::string resolvedPath = MapManager::ResolveTilePath(matchedTile->type, matchedTile->requested);
                        if (resolvedPath.empty() || !fs::exists(resolvedPath)) {
                            LOG_WARN("Tile path not found for color ({},{},{}): requested='{}' resolved='{}'",
                                     r, g, b, matchedTile->requested, resolvedPath);
                        }
                        map.AddTile(x, y, matchedTile->type, resolvedPath);
                    }
                } else {
                    // If not matched, prefer a Stair asset if present; otherwise try Brick fallback.
                    if (haveStairAsset) {
                        map.AddTile(x, y, Cell::Brick, stairResolved);
                    } else {
                        // fallback: try Brick.png (MapManager will resolve)
                        std::string resolvedBrick = MapManager::ResolveTilePath(Cell::Brick, "Brick.png");
                        map.AddTile(x, y, Cell::Brick, resolvedBrick);
                    }
                }
            }

            // =========================
            // MIDDLE 1/3 = ENTITIES
            // =========================
            if (!GetPixelRGBA(surface, x, y + layerHeight, r, g, b, a)) continue;
            if (a > 0) {
                if (r == 255 && g == 0 && b == 0) {
                    mario.m_Transform.translation = ComputeGroundedSpawnPosition(
                        map, x, y, mario.GetHalfExtents().y
                    );
                    spawnFound = true;
                } else if (r == 182 && g == 73 && b == 0) {
                    goombaMask[x][y] = 1;
                }
            }

            // =========================
            // BOTTOM 1/3 = BACKGROUND
            // =========================
            if (!GetPixelRGBA(surface, x, y + 2 * layerHeight, r, g, b, a)) continue;
            if (a > 0) {
                // If the pixel is a sky-like blue, prefer using the solid background color rather than tiled background art.
                if (IsSkyColor(r, g, b)) {
                    continue;
                }

                if (r == 255 && g == 255 && b == 255) {
                    cloudMask[x][y] = 1;
                    continue;
                }

                if (IsBushColor(r, g, b)) {
                    bushMask[x][y] = 1;
                    continue;
                }

                if (IsFlagColor(r, g, b)) {
                    flagMask[x][y] = 1;
                    continue;
                }

                if (IsMountainColor(r, g, b)) {
                    mountainMask[x][y] = 1;
                    continue;
                }
            }
        }
    }

    auto AddComponentSprites = [&](const std::vector<std::vector<char>>& mask,
                                   bool isBackground,
                                   Cell type,
                                   const std::string& resolvedPath) {
        std::vector<std::vector<char>> visited(width, std::vector<char>(layerHeight, 0));
        for (int sx = 0; sx < width; ++sx) {
            for (int sy = 0; sy < layerHeight; ++sy) {
                if (!mask[sx][sy] || visited[sx][sy]) continue;

                int minX = sx;
                int maxX = sx;
                int minY = sy;
                int maxY = sy;
                std::queue<std::pair<int, int>> q;
                q.push({sx, sy});
                visited[sx][sy] = 1;

                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();
                    minX = std::min(minX, cx);
                    maxX = std::max(maxX, cx);
                    minY = std::min(minY, cy);
                    maxY = std::max(maxY, cy);

                    const int dx[4] = {1, -1, 0, 0};
                    const int dy[4] = {0, 0, 1, -1};
                    for (int i = 0; i < 4; ++i) {
                        int nx = cx + dx[i];
                        int ny = cy + dy[i];
                        if (nx < 0 || nx >= width || ny < 0 || ny >= layerHeight) continue;
                        if (visited[nx][ny] || !mask[nx][ny]) continue;
                        visited[nx][ny] = 1;
                        q.push({nx, ny});
                    }
                }

                int spanX = maxX - minX + 1;
                int spanY = maxY - minY + 1;
                if (isBackground) {
                    map.AddBackgroundSprite(minX, minY, spanX, spanY, resolvedPath);
                } else {
                    map.AddTileSprite(minX, minY, spanX, spanY, type, resolvedPath);
                }
            }
        }
    };

    if (!pipeResolved.empty() && fs::exists(pipeResolved)) {
        AddComponentSprites(pipeMask, false, Cell::Pipe, pipeResolved);
    }
    if (!mountainResolved.empty() && fs::exists(mountainResolved)) {
        AddComponentSprites(mountainMask, true, Cell::Empty, mountainResolved);
    }
    if (!bushResolved.empty() && fs::exists(bushResolved)) {
        AddComponentSprites(bushMask, true, Cell::Empty, bushResolved);
    }
    if (!cloudResolved.empty() && fs::exists(cloudResolved)) {
        AddComponentSprites(cloudMask, true, Cell::Empty, cloudResolved);
    }

    std::vector<std::vector<char>> visitedFlag(width, std::vector<char>(layerHeight, 0));
    for (int sx = 0; sx < width; ++sx) {
        for (int sy = 0; sy < layerHeight; ++sy) {
            if (!flagMask[sx][sy] || visitedFlag[sx][sy]) continue;

            int minX = sx, maxX = sx, minY = sy, maxY = sy;
            std::queue<std::pair<int, int>> q;
            q.push({sx, sy});
            visitedFlag[sx][sy] = 1;
            while (!q.empty()) {
                auto [cx, cy] = q.front();
                q.pop();
                minX = std::min(minX, cx);
                maxX = std::max(maxX, cx);
                minY = std::min(minY, cy);
                maxY = std::max(maxY, cy);
                const int dx[4] = {1, -1, 0, 0};
                const int dy[4] = {0, 0, 1, -1};
                for (int i = 0; i < 4; ++i) {
                    int nx = cx + dx[i];
                    int ny = cy + dy[i];
                    if (nx < 0 || nx >= width || ny < 0 || ny >= layerHeight) continue;
                    if (visitedFlag[nx][ny] || !flagMask[nx][ny]) continue;
                    visitedFlag[nx][ny] = 1;
                    q.push({nx, ny});
                }
            }

            map.AddBackgroundSprite(minX, minY, 1, maxY - minY + 1,
                                    MapManager::ResolveBackgroundPath("flagstick.png"));
            map.AddBackgroundSprite(minX, minY, 1, 1,
                                    MapManager::ResolveBackgroundPath("dot.png"));
            if (minY + 1 <= maxY) {
                map.AddBackgroundSprite(minX, minY + 1, 1, 2,
                                        MapManager::ResolveBackgroundPath("flag.png"));
            }
            map.SetGoal(map.GetWorldLeft() + minX * map.GetTileSize() + map.GetTileSize() * 0.5f);
        }
    }

    if (enemy_spawns != nullptr) {
        std::vector<std::vector<char>> visited(width, std::vector<char>(layerHeight, 0));
        for (int sx = 0; sx < width; ++sx) {
            for (int sy = 0; sy < layerHeight; ++sy) {
                if (!goombaMask[sx][sy] || visited[sx][sy]) continue;

                int minX = sx;
                int maxX = sx;
                int minY = sy;
                int maxY = sy;
                std::queue<std::pair<int, int>> q;
                q.push({sx, sy});
                visited[sx][sy] = 1;

                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();
                    minX = std::min(minX, cx);
                    maxX = std::max(maxX, cx);
                    minY = std::min(minY, cy);
                    maxY = std::max(maxY, cy);

                    const int dx[4] = {1, -1, 0, 0};
                    const int dy[4] = {0, 0, 1, -1};
                    for (int i = 0; i < 4; ++i) {
                        int nx = cx + dx[i];
                        int ny = cy + dy[i];
                        if (nx < 0 || nx >= width || ny < 0 || ny >= layerHeight) continue;
                        if (visited[nx][ny] || !goombaMask[nx][ny]) continue;
                        visited[nx][ny] = 1;
                        q.push({nx, ny});
                    }
                }

                const int centerX = (minX + maxX) / 2;
                enemy_spawns->push_back(
                    ComputeGroundedSpawnPosition(map, centerX, maxY, 20.0f)
                );
            }
        }
    }

    // Post-process: replace side-by-side neighbors of QuestionBlock with Brick.
    auto FindTilePathForType = [&](Cell t)->std::string {
        for (const auto& kv : tileMap) {
            if (kv.second.type == t && !kv.second.requested.empty()) {
                std::string resolved = MapManager::ResolveTilePath(t, kv.second.requested);
                if (!resolved.empty()) return resolved;
            }
        }
        // fallback: ask MapManager for the default by type (attempt common filenames)
        switch (t) {
            case Cell::Brick: return MapManager::ResolveTilePath(t, "Brick.png");
            case Cell::Pipe:  return MapManager::ResolveTilePath(t, "Pipe.png");
            case Cell::Wall:  return MapManager::ResolveTilePath(t, "Ground.png");
            default: return std::string();
        }
    };

    std::string brickPath = FindTilePathForType(Cell::Brick);
    std::string hardBlockPath = MapManager::ResolveTilePath(Cell::Brick, "HardBlock.png");
    bool haveHardBlockAsset = !hardBlockPath.empty() && fs::exists(hardBlockPath);
    if (haveHardBlockAsset) {
        std::vector<std::vector<char>> visited(width, std::vector<char>(layerHeight, 0));
        for (int sx = 0; sx < width; ++sx) {
            for (int sy = 0; sy < layerHeight; ++sy) {
                if (visited[sx][sy] || map.GetCell(sx, sy) != Cell::Brick) continue;

                std::vector<std::pair<int, int>> comp;
                std::queue<std::pair<int, int>> q;
                q.push({sx, sy});
                visited[sx][sy] = 1;

                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();
                    comp.push_back({cx, cy});

                    const int dx[4] = {1, -1, 0, 0};
                    const int dy[4] = {0, 0, 1, -1};
                    for (int i = 0; i < 4; ++i) {
                        int nx = cx + dx[i];
                        int ny = cy + dy[i];
                        if (nx < 0 || nx >= width || ny < 0 || ny >= layerHeight) continue;
                        if (visited[nx][ny] || map.GetCell(nx, ny) != Cell::Brick) continue;
                        visited[nx][ny] = 1;
                        q.push({nx, ny});
                    }
                }

                if (static_cast<int>(comp.size()) < 6) continue;

                int minX = width;
                int maxX = -1;
                for (const auto& p : comp) {
                    minX = std::min(minX, p.first);
                    maxX = std::max(maxX, p.first);
                }
                if (minX < static_cast<int>(width * 0.6f)) continue;

                int cols = maxX - minX + 1;
                if (cols < 3) continue;

                std::vector<int> topY(cols, layerHeight + 1);
                std::vector<int> colCount(cols, 0);
                for (const auto& p : comp) {
                    int cx = p.first - minX;
                    topY[cx] = std::min(topY[cx], p.second);
                    colCount[cx]++;
                }

                std::vector<int> seq;
                for (int i = 0; i < cols; ++i) {
                    if (colCount[i] > 0) seq.push_back(topY[i]);
                }
                if (static_cast<int>(seq.size()) < 3) continue;

                int sign = 0;
                bool monotonic = true;
                for (size_t i = 1; i < seq.size(); ++i) {
                    int d = seq[i] - seq[i - 1];
                    if (d == 0) continue;
                    int s = (d > 0) ? 1 : -1;
                    if (sign == 0) sign = s;
                    else if (sign != s) {
                        monotonic = false;
                        break;
                    }
                }
                if (!monotonic) continue;

                for (const auto& p : comp) {
                    map.AddTile(p.first, p.second, Cell::Brick, hardBlockPath);
                    isHard[p.first][p.second] = 1;
                }
            }
        }
    }

    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < layerHeight; ++y) {
            Cell c = map.GetCell(x, y);
            if (c == Cell::QuestionBlock) {
                for (int dx : {-1, 1}) {
                    int nx = x + dx;
                    if (nx < 0 || nx >= width) continue;
                    if (isHard[nx][y]) continue;
                    Cell neighbor = map.GetCell(nx, y);
                    if (neighbor == Cell::QuestionBlock || neighbor == Cell::Pipe) continue;
                    if (!brickPath.empty()) {
                        LOG_INFO("Replacing neighbor at ({}, {}) with Brick", nx, y);
                        map.AddTile(nx, y, Cell::Brick, brickPath);
                    }
                }
            }
        }
    }

    SDL_FreeSurface(surface);

    if (!spawnFound) {
        LOG_ERROR("No red Mario spawn pixel found in entity layer.");
    }

    return spawnFound;
}

