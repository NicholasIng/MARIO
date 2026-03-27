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

bool convert_sketch(
    const std::string& path,
    MapManager& map,
    Mario& mario,
    Util::Color& background_color
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

    // tile mappings: packed RGB -> pair(Cell type, requested filename)
    struct TileEntry { Cell type; std::string requested; };
    std::unordered_map<uint32_t, TileEntry> tileMap = {
        { PackRGB(0,   0,   0),   { Cell::Wall,          "Ground.png" } },
        { PackRGB(182, 73,  0),   { Cell::Brick,         "Brick.png" } },
        { PackRGB(255, 255, 0),   { Cell::Coin,          "Coin.png" } },
        { PackRGB(255, 73,  85),  { Cell::QuestionBlock, "Question.png" } },
        { PackRGB(255, 146, 85),  { Cell::QuestionBlock, "Question.png" } },
        { PackRGB(0,   146, 0),   { Cell::Pipe,          "Pipe.png" } },
        { PackRGB(0,   182, 0),   { Cell::Pipe,          "Pipe.png" } },
        { PackRGB(0,   219, 0),   { Cell::Pipe,          "Pipe.png" } }
    };

    // background mappings: packed RGB -> requested filename
    std::unordered_map<uint32_t, std::string> bgMap = {
        { PackRGB(255, 255, 255), "Cloud.png" },
        { PackRGB(146, 219, 0),   "Grass.png" },
        { PackRGB(0,   73,  0),   "HillOutline.png" },
        { PackRGB(0,   109, 0),   "HillFill.png" },
        { PackRGB(109, 255, 85),  "Flagpole.png" }
    };

    bool spawnFound = false;

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
                if (it != tileMap.end()) {
                    // resolve with MapManager to get the canonical path used by the rest of the app
                    std::string resolvedPath = MapManager::ResolveTilePath(it->second.type, it->second.requested);
                    if (resolvedPath.empty() || !fs::exists(resolvedPath)) {
                        LOG_WARN("Tile path not found for color ({},{},{}): requested='{}' resolved='{}'",
                                 r, g, b, it->second.requested, resolvedPath);
                    }
                    map.AddTile(x, y, it->second.type, resolvedPath);
                }
            }

            // =========================
            // MIDDLE 1/3 = ENTITIES
            // =========================
            if (!GetPixelRGBA(surface, x, y + layerHeight, r, g, b, a)) continue;
            if (a > 0 && r == 255 && g == 0 && b == 0) {
                float tileSize = map.GetTileSize();
                float worldX = map.GetWorldLeft() + x * tileSize + tileSize / 2.0f;
                float worldY = (layerHeight * tileSize) / 2.0f - y * tileSize - tileSize / 2.0f;
                mario.m_Transform.translation = { worldX, worldY };
                spawnFound = true;
            }

            // =========================
            // BOTTOM 1/3 = BACKGROUND
            // =========================
            if (!GetPixelRGBA(surface, x, y + 2 * layerHeight, r, g, b, a)) continue;
            if (a > 0) {
                if (x == 0 && y == 0) {
                    background_color = Util::Color(r, g, b, a);
                }
                uint32_t key = PackRGB(r,g,b);
                auto itb = bgMap.find(key);
                if (itb != bgMap.end()) {
                    std::string resolvedBg = MapManager::ResolveBackgroundPath(itb->second);
                    if (resolvedBg.empty() || !fs::exists(resolvedBg)) {
                        LOG_WARN("Background path not found for color ({},{},{}): requested='{}' resolved='{}'",
                                 r, g, b, itb->second, resolvedBg);
                    }
                    map.AddBackgroundTile(x, y, resolvedBg);
                }
            }
        }
    }

    // Post-process: replace side-by-side neighbors of QuestionBlock with Brick (if not pipe/question)
    auto FindTilePathForType = [&](Cell t)->std::string {
        // ask MapManager for canonical tile path for given type by trying our known filenames
        for (const auto& kv : tileMap) {
            if (kv.second.type == t && !kv.second.requested.empty()) {
                std::string resolved = MapManager::ResolveTilePath(t, kv.second.requested);
                if (!resolved.empty()) return resolved;
            }
        }
        return std::string();
    };

    std::string brickPath = FindTilePathForType(Cell::Brick);
    std::string pipePath  = FindTilePathForType(Cell::Pipe);

    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < layerHeight; ++y) {
            Cell c = map.GetCell(x, y);
            if (c == Cell::QuestionBlock) {
                for (int dx : {-1, 1}) {
                    int nx = x + dx;
                    if (nx < 0 || nx >= width) continue;
                    Cell neighbor = map.GetCell(nx, y);
                    if (neighbor == Cell::QuestionBlock || neighbor == Cell::Pipe) continue;
                    if (!brickPath.empty()) {
                        LOG_INFO("Replacing neighbor at ({}, {}) with Brick", nx, y);
                        map.AddTile(nx, y, Cell::Brick, brickPath);
                    }
                }
            } else if (c == Cell::Pipe) {
                int below = y + 1;
                if (below < layerHeight && map.GetCell(x, below) == Cell::Empty && !pipePath.empty()) {
                    LOG_INFO("Auto-extending Pipe at ({}, {}) downward", x, below);
                    map.AddTile(x, below, Cell::Pipe, pipePath);
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
