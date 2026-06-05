#include "ConvertSketch.hpp"
#include "Util/Logger.hpp"
#include "MapManager.hpp"
#include "Enemy.hpp"
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
#include <functional>

namespace fs = std::filesystem;

namespace {

constexpr Uint8 SKY_BLUE_R = 90;
constexpr Uint8 SKY_BLUE_G = 147;
constexpr Uint8 SKY_BLUE_B = 235;

}

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

static bool IsCastleColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(255, 216, 0);
}

static bool IsPipeForkedColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(0, 255, 176);
}

static bool IsMovingPlatformColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(255, 205, 0);
}

static bool IsWoodColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(255, 115, 0);
}

static bool IsTreeColor(Uint8 r, Uint8 g, Uint8 b) {
    return PackRGB(r, g, b) == PackRGB(71, 255, 40);
}

static bool FindMarioSpawnMarker(SDL_Surface* surface,
                                 int layerHeight,
                                 int& outGridX,
                                 int& outEntityGridY,
                                 bool& outFoundOutsideEntityLayer) {
    if (!surface || layerHeight <= 0) return false;

    outFoundOutsideEntityLayer = false;

    for (int x = 0; x < surface->w; ++x) {
        for (int y = 0; y < layerHeight; ++y) {
            Uint8 r, g, b, a;
            if (!GetPixelRGBA(surface, x, y + layerHeight, r, g, b, a)) continue;
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
            if (!GetPixelRGBA(surface, x, y, r, g, b, a)) continue;
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

static glm::vec2 ComputeGroundedSpawnPosition(const MapManager& map,
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
                return { worldX, tileTop + halfHeight };
            }
        }
    }

    const float fallbackY = (map.GetHeight() * tileSize) / 2.0f - entityGridY * tileSize + tileSize / 2.0f;
    return { worldX, fallbackY };
}

static glm::vec2 ComputeMarkerSpawnPosition(const MapManager& map,
                                            int gridX,
                                            int entityGridY) {
    const float tileSize = map.GetTileSize();
    const int clampedGridX = std::clamp(gridX, 0, std::max(0, map.GetWidth() - 1));
    const int clampedGridY = std::clamp(entityGridY, 0, std::max(0, map.GetHeight() - 1));
    const float worldX = map.GetWorldLeft() + clampedGridX * tileSize + tileSize / 2.0f;
    const float worldY = (map.GetHeight() * tileSize) / 2.0f - clampedGridY * tileSize - tileSize / 2.0f;
    return { worldX, worldY };
}

bool convert_sketch(
    const std::string& path,
    MapManager& map,
    Mario& mario,
    Util::Color& background_color,
    std::vector<EnemySpawnInfo>* enemy_spawns
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

    const std::string loweredPath = ToLower(path);
    const bool isOutdoorExitArea =
        loweredPath.find("levelsketch1-1") != std::string::npos ||
        loweredPath.find("outdoorexitsketch") != std::string::npos;
    const bool isTransitionSceneSketch =
        loweredPath.find("transition1") != std::string::npos;
    const bool isUndergroundLevel =
        !isOutdoorExitArea &&
        !isTransitionSceneSketch &&
        (loweredPath.find("levelsketch1.png") != std::string::npos ||
         loweredPath.find("1-2") != std::string::npos);
    const bool shouldUseMarkerSpawn = loweredPath.find("levelsketch1.png") != std::string::npos;
    map.SetUndergroundTheme(isUndergroundLevel);

    background_color = isUndergroundLevel
        ? Util::Color(0, 0, 0, 255)
        : Util::Color(SKY_BLUE_R, SKY_BLUE_G, SKY_BLUE_B, 255);

    // tile mappings: packed RGB -> pair(Cell type, requested filename)
    struct TileEntry {
        Cell type;
        std::string requested;
        LootType questionLoot = LootType::Coin;
        int questionHitCount = 1;
        bool renderAsBrick = false;
        bool hiddenUntilHit = false;
    };
    std::unordered_map<uint32_t, TileEntry> tileMap = {
        { PackRGB(0,   0,   0),   { Cell::Wall,          isUndergroundLevel ? "SMB_Ground_Underground.png" : "Ground.png" } },
        { PackRGB(182, 73,  0),   { Cell::Brick,         isUndergroundLevel ? "SMB_Underground_Brick_Block.png" : "Brick.png" } },
        { PackRGB(146, 73,  0),   { (isUndergroundLevel || isOutdoorExitArea) ? Cell::Wall : Cell::Brick, isUndergroundLevel ? "SMB_Underground_Hard_Block.png" : (isOutdoorExitArea ? "HardBlock.png" : "Brick.png") } },
        { PackRGB(255, 255, 0),   { Cell::Coin,          isUndergroundLevel ? "ug_coin1.png" : "coin1.png" } },
        { PackRGB(255, 73,  85),  { Cell::QuestionBlock, isUndergroundLevel ? "ug_question1.png" : "Question.png", LootType::ProgressivePowerUp } },
        { PackRGB(255, 135, 143), { Cell::QuestionBlock, isUndergroundLevel ? "SMB_Underground_Brick_Block.png" : "Brick.png", LootType::RedMushroom, 1, true } },
        { PackRGB(255, 146, 85),  { Cell::QuestionBlock, isUndergroundLevel ? "ug_question1.png" : "Question.png", LootType::Coin } },
        { PackRGB(0,   20, 255),  { Cell::QuestionBlock, isUndergroundLevel ? "SMB_Underground_Brick_Block.png" : "Brick.png", LootType::Star, 1, true } },
        { PackRGB(0,   100, 0),   { Cell::QuestionBlock, isUndergroundLevel ? "SMB_Underground_Brick_Block.png" : "Brick.png", LootType::GreenMushroom, 1, true } },
        { PackRGB(121, 255, 107), { Cell::QuestionBlock, isUndergroundLevel ? "ug_question1.png" : "Question.png", LootType::GreenMushroom, 1, false, true } },
        { PackRGB(243, 125, 45),  { Cell::QuestionBlock, isUndergroundLevel ? "SMB_Underground_Brick_Block.png" : "Brick.png", LootType::Coin, 11, true } },
        { PackRGB(0,   146, 0),   { Cell::Pipe,          "Pipe.png" } },
        { PackRGB(0,   182, 0),   { Cell::Pipe,          "Pipe.png" } },
        { PackRGB(0,   219, 0),   { Cell::Pipe,          "Pipe.png" } },
        // optional stair-colored pixels: if the resources contain a "Stair.png"
        { PackRGB(128, 128, 128), { Cell::Brick,         "Stair.png" } }, // common gray stair
        { PackRGB(150,  75,   0), { Cell::Brick,         "Stair.png" } }  // brown-ish stair
    };

    int marioSpawnX = -1;
    int marioSpawnY = -1;
    bool spawnMarkerFoundOutsideEntityLayer = false;

    // pre-resolve stair fallback: if a "Stair.png" exists (under resources) use it for unmatched top-layer tiles
    std::string stairResolved = MapManager::ResolveTilePath(
        Cell::Brick,
        isUndergroundLevel ? "SMB_Underground_Hard_Block.png" : "Stair.png"
    );
    bool haveStairAsset = false;
    if (!stairResolved.empty() && fs::exists(stairResolved)) {
        haveStairAsset = true;
        LOG_INFO("Using Stair asset: {}", stairResolved);
    }

    auto ResolvePipePiece = [](const std::string& filename) {
        const fs::path pipePath = MapManager::ResourceRoot() / "image" / "Pipes" / filename;
        if (fs::exists(pipePath)) {
            return pipePath.string();
        }
        return MapManager::ResolveTilePath(Cell::Pipe, filename);
    };

    const std::string pipeTopLeft = ResolvePipePiece("pipetop_left.png");
    const std::string pipeTopRight = ResolvePipePiece("pipetop_right.png");
    const std::string pipeBottomLeft = ResolvePipePiece("pipebottom_left.png");
    const std::string pipeBottomRight = ResolvePipePiece("pipebottom_right.png");
    const std::string pipeBodyResolved = MapManager::ResolveTilePath(Cell::Pipe, "pipebody.png");
    const std::string pipeResolved = MapManager::ResolveTilePath(Cell::Pipe, "Pipe.png");
    const std::string warpPipeBottomResolved = MapManager::ResolveBackgroundPath("WarpPipeBottom.png");
    const std::string exitPipeTopLeft = ResolvePipePiece("pipe_tl.png");
    const std::string exitPipeTopRight = ResolvePipePiece("pipe_tr.png");
    const std::string exitPipeBodyLeft = ResolvePipePiece("pipe_bl.png");
    const std::string exitPipeBodyRight = ResolvePipePiece("pipe_br.png");
    const std::string exitMouthTopLeft = ResolvePipePiece("mpipe_tl.png");
    const std::string exitMouthTopMiddle = ResolvePipePiece("mpipe_mt.png");
    const std::string exitMouthTopRight = ResolvePipePiece("mpipe_tr.png");
    const std::string exitMouthBottomLeft = ResolvePipePiece("mpipe_bl.png");
    const std::string exitMouthBottomMiddle = ResolvePipePiece("mpipe_mb.png");
    const std::string exitMouthBottomRight = ResolvePipePiece("mpipe_br.png");
    std::string mountainResolved = isUndergroundLevel ? "" : MapManager::ResolveBackgroundPath("mountains.png");
    std::string bushResolved = isUndergroundLevel ? "" : MapManager::ResolveBackgroundPath("Bush.png");
    std::string cloudResolved = isUndergroundLevel ? "" : MapManager::ResolveBackgroundPath("Clouds_2.png");
    std::string castleResolved = MapManager::ResolveBackgroundPath("castle1.png");
    std::string woodResolved = isUndergroundLevel ? "" : MapManager::ResolveBackgroundPath("wood.png");
    std::string treeLeftResolved = isUndergroundLevel ? "" : MapManager::ResolveBackgroundPath("tree_left.png");
    std::string treeMiddleResolved = isUndergroundLevel ? "" : MapManager::ResolveBackgroundPath("tree_middle.png");
    std::string treeRightResolved = isUndergroundLevel ? "" : MapManager::ResolveBackgroundPath("tree_right.png");
    std::string pipeForkedResolved = MapManager::ResolveBackgroundPath("WarpPipeForked.png");
    std::vector<std::vector<char>> pipeMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> mountainMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> bushMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> cloudMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> woodMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> treeMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> flagMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> castleMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> pipeForkedMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> forkedPipeBottomMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> forkedPipeBodyMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> movingPlatformMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> goombaMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> koopaMask(width, std::vector<char>(layerHeight, 0));
    std::vector<std::vector<char>> redKoopaMask(width, std::vector<char>(layerHeight, 0));
    std::vector<glm::vec2> venusSpawnPositions;
    bool undergroundExitPipeVisualAdded = false;

    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < layerHeight; ++y) {
            Uint8 r,g,b,a;

            // =========================
            // TOP 1/3 = BLOCKS
            // =========================
            if (!GetPixelRGBA(surface, x, y, r, g, b, a)) continue;
            if (a > 0) {
                if (IsCastleColor(r, g, b)) {
                    castleMask[x][y] = 1;
                    continue;
                }
                if (IsPipeForkedColor(r, g, b)) {
                    pipeForkedMask[x][y] = 1;
                    continue;
                }
                if (IsMovingPlatformColor(r, g, b)) {
                    movingPlatformMask[x][y] = 1;
                    continue;
                }
                if (IsWoodColor(r, g, b)) {
                    woodMask[x][y] = 1;
                    continue;
                }
                if (IsTreeColor(r, g, b)) {
                    treeMask[x][y] = 1;
                    continue;
                }

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
                        if (r == 0 && g == 219 && b == 0) {
                            forkedPipeBottomMask[x][y] = 1;
                        } else if (r == 0 && g == 182 && b == 0) {
                            forkedPipeBodyMask[x][y] = 1;
                        }
                    } else {
                        std::string resolvedPath = MapManager::ResolveTilePath(matchedTile->type, matchedTile->requested);
                        if (resolvedPath.empty() || !fs::exists(resolvedPath)) {
                            LOG_WARN("Tile path not found for color ({},{},{}): requested='{}' resolved='{}'",
                                     r, g, b, matchedTile->requested, resolvedPath);
                        }
                        map.AddTile(x, y, matchedTile->type, resolvedPath);
                        if (matchedTile->type == Cell::QuestionBlock) {
                            map.SetQuestionBlockLoot(x, y, matchedTile->questionLoot);
                            map.SetQuestionBlockHitCount(x, y, matchedTile->questionHitCount);
                            if (matchedTile->renderAsBrick) {
                                map.SetQuestionBlockStaticTexture(x, y, matchedTile->requested);
                            }
                            if (matchedTile->hiddenUntilHit) {
                                map.SetQuestionBlockHidden(x, y, true);
                            }
                        }
                    }
                } else {
                    // If not matched, prefer a Stair asset if present; otherwise try Brick fallback.
                    if (haveStairAsset) {
                        map.AddTile(x, y, Cell::Brick, stairResolved);
                    } else {
                        // fallback: try Brick.png (MapManager will resolve)
                        std::string resolvedBrick = MapManager::ResolveTilePath(
                            Cell::Brick,
                            isUndergroundLevel ? "SMB_Underground_Brick_Block.png" : "Brick.png"
                        );
                        map.AddTile(x, y, Cell::Brick, resolvedBrick);
                    }
                }
            }

            // =========================
            // MIDDLE 1/3 = ENTITIES
            // =========================
            if (!GetPixelRGBA(surface, x, y + layerHeight, r, g, b, a)) continue;
            if (a > 0) {
                if (IsCastleColor(r, g, b)) {
                    castleMask[x][y] = 1;
                } else if (IsPipeForkedColor(r, g, b)) {
                    pipeForkedMask[x][y] = 1;
                } else if (r == 0 && g == 255 && b == 42) {
                    koopaMask[x][y] = 1;
                } else if (r == 255 && g == 127 && b == 0) {
                    redKoopaMask[x][y] = 1;
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

                if (IsCastleColor(r, g, b)) {
                    castleMask[x][y] = 1;
                    continue;
                }

                if (IsPipeForkedColor(r, g, b)) {
                    pipeForkedMask[x][y] = 1;
                    continue;
                }

                if (IsWoodColor(r, g, b)) {
                    woodMask[x][y] = 1;
                    continue;
                }

                if (IsTreeColor(r, g, b)) {
                    treeMask[x][y] = 1;
                    continue;
                }

                if (IsMountainColor(r, g, b)) {
                    mountainMask[x][y] = 1;
                    continue;
                }
            }
        }
    }

    const bool spawnMarkerFound = FindMarioSpawnMarker(
        surface,
        layerHeight,
        marioSpawnX,
        marioSpawnY,
        spawnMarkerFoundOutsideEntityLayer
    );
    if (spawnMarkerFoundOutsideEntityLayer) {
        LOG_WARN("Mario spawn marker was found outside the entity layer; using its wrapped entity position.");
    }

    auto AddComponentSprites = [&](const std::vector<std::vector<char>>& mask,
                                   bool isBackground,
                                   Cell type,
                                   const std::string& resolvedPath,
                                   const std::function<void(int, int, int, int)>& onComponent = {}) {
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
                if (onComponent) {
                    onComponent(minX, minY, spanX, spanY);
                }
            }
        }
    };

    auto AddMarkerScaledSprite = [&](const std::vector<std::vector<char>>& mask,
                                     const std::string& resolvedPath,
                                     bool drawInForeground = false,
                                     const std::function<void(int, int, int, int, int, int)>& onComponent = {}) {
        if (resolvedPath.empty() || !fs::exists(resolvedPath)) {
            return;
        }

        Util::Image image(resolvedPath);
        const glm::vec2 imageSize = image.GetSize();
        if (imageSize.x <= 0.0f || imageSize.y <= 0.0f) {
            return;
        }

        const int spriteTileWidth = std::max(1, static_cast<int>(std::round((imageSize.x * 3.0f) / map.GetTileSize())));
        const int spriteTileHeight = std::max(1, static_cast<int>(std::round((imageSize.y * 3.0f) / map.GetTileSize())));

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
                        const int nx = cx + dx[i];
                        const int ny = cy + dy[i];
                        if (nx < 0 || nx >= width || ny < 0 || ny >= layerHeight) continue;
                        if (visited[nx][ny] || !mask[nx][ny]) continue;
                        visited[nx][ny] = 1;
                        q.push({nx, ny});
                    }
                }

                const int markerCenterX = (minX + maxX) / 2;
                const int markerBaseY = maxY;
                const int startX = std::clamp(markerCenterX - spriteTileWidth / 2, 0, std::max(0, width - spriteTileWidth));
                const int startY = std::clamp(markerBaseY - spriteTileHeight + 1, 0, std::max(0, layerHeight - spriteTileHeight));

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
    };

    const bool havePipeSet =
        !pipeTopLeft.empty() && fs::exists(pipeTopLeft) &&
        !pipeTopRight.empty() && fs::exists(pipeTopRight) &&
        !pipeBottomLeft.empty() && fs::exists(pipeBottomLeft) &&
        !pipeBottomRight.empty() && fs::exists(pipeBottomRight);
    const bool haveWarpPipeBottom =
        !warpPipeBottomResolved.empty() && fs::exists(warpPipeBottomResolved);
    const bool havePipeBodyTexture =
        !pipeBodyResolved.empty() && fs::exists(pipeBodyResolved);
    const bool haveExitPipeSet =
        !exitPipeTopLeft.empty() && fs::exists(exitPipeTopLeft) &&
        !exitPipeTopRight.empty() && fs::exists(exitPipeTopRight) &&
        !exitPipeBodyLeft.empty() && fs::exists(exitPipeBodyLeft) &&
        !exitPipeBodyRight.empty() && fs::exists(exitPipeBodyRight) &&
        !exitMouthTopLeft.empty() && fs::exists(exitMouthTopLeft) &&
        !exitMouthTopMiddle.empty() && fs::exists(exitMouthTopMiddle) &&
        !exitMouthTopRight.empty() && fs::exists(exitMouthTopRight) &&
        !exitMouthBottomLeft.empty() && fs::exists(exitMouthBottomLeft) &&
        !exitMouthBottomMiddle.empty() && fs::exists(exitMouthBottomMiddle) &&
        !exitMouthBottomRight.empty() && fs::exists(exitMouthBottomRight);
    auto AddForegroundPipeTile = [&](int gridX, int gridY, const std::string& texturePath) {
        if (gridX < 0 || gridX >= width || gridY < 0 || gridY >= layerHeight) {
            return;
        }
        map.AddTile(gridX, gridY, Cell::Pipe, texturePath);
        map.AddForegroundSprite(gridX, gridY, 1, 1, texturePath);
    };
    auto GetMarkerSpriteTileSize = [&](const std::string& resolvedPath) {
        Util::Image image(resolvedPath);
        const glm::vec2 imageSize = image.GetSize();
        return std::pair<int, int>{
            std::max(1, static_cast<int>(std::round((imageSize.x * 3.0f) / map.GetTileSize()))),
            std::max(1, static_cast<int>(std::round((imageSize.y * 3.0f) / map.GetTileSize())))
        };
    };

    if (havePipeSet) {
        std::vector<std::vector<char>> visited(width, std::vector<char>(layerHeight, 0));
        for (int sx = 0; sx < width; ++sx) {
            for (int sy = 0; sy < layerHeight; ++sy) {
                if (!pipeMask[sx][sy] || visited[sx][sy]) continue;

                int minX = sx;
                int maxX = sx;
                int minY = sy;
                int maxY = sy;
                std::queue<std::pair<int, int>> q;
                q.push({sx, sy});
                visited[sx][sy] = 1;

                std::vector<std::pair<int, int>> cells;
                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();
                    cells.push_back({cx, cy});
                    minX = std::min(minX, cx);
                    maxX = std::max(maxX, cx);
                    minY = std::min(minY, cy);
                    maxY = std::max(maxY, cy);

                    const int dx[4] = {1, -1, 0, 0};
                    const int dy[4] = {0, 0, 1, -1};
                    for (int i = 0; i < 4; ++i) {
                        const int nx = cx + dx[i];
                        const int ny = cy + dy[i];
                        if (nx < 0 || nx >= width || ny < 0 || ny >= layerHeight) continue;
                        if (visited[nx][ny] || !pipeMask[nx][ny]) continue;
                        visited[nx][ny] = 1;
                        q.push({nx, ny});
                    }
                }
                const int spanX = maxX - minX + 1;
                bool handledUndergroundExitPipe = false;
                if (isUndergroundLevel) {
                    bool hasForkedBottomMarker = false;
                    int forkedBottomMinX = width;
                    int forkedBottomMaxX = -1;
                    int forkedBottomMinY = layerHeight;
                    int forkedBottomMaxY = -1;
                    bool hasForkedBodyMarker = false;
                    int forkedBodyMinX = width;
                    int forkedBodyMaxX = -1;
                    int forkedBodyMinY = layerHeight;
                    int forkedBodyMaxY = -1;

                    for (const auto& [cellX, cellY] : cells) {
                        if (forkedPipeBottomMask[cellX][cellY]) {
                            hasForkedBottomMarker = true;
                            forkedBottomMinX = std::min(forkedBottomMinX, cellX);
                            forkedBottomMaxX = std::max(forkedBottomMaxX, cellX);
                            forkedBottomMinY = std::min(forkedBottomMinY, cellY);
                            forkedBottomMaxY = std::max(forkedBottomMaxY, cellY);
                        }
                        if (forkedPipeBodyMask[cellX][cellY]) {
                            hasForkedBodyMarker = true;
                            forkedBodyMinX = std::min(forkedBodyMinX, cellX);
                            forkedBodyMaxX = std::max(forkedBodyMaxX, cellX);
                            forkedBodyMinY = std::min(forkedBodyMinY, cellY);
                            forkedBodyMaxY = std::max(forkedBodyMaxY, cellY);
                        }
                    }

                    if (!undergroundExitPipeVisualAdded &&
                        hasForkedBottomMarker &&
                        hasForkedBodyMarker &&
                        haveWarpPipeBottom &&
                        havePipeBodyTexture) {
                        const auto [bottomTileWidth, bottomTileHeight] = GetMarkerSpriteTileSize(warpPipeBottomResolved);
                        const int bottomStartX =
                            std::clamp(forkedBodyMinX - bottomTileWidth + 2, 0, std::max(0, width - bottomTileWidth));
                        const int bottomStartY =
                            std::clamp(forkedBottomMaxY - bottomTileHeight + 1, 0, std::max(0, layerHeight - bottomTileHeight));
                        const int bodyTileWidth = std::max(1, forkedBodyMaxX - forkedBodyMinX + 1);
                        const int bodySegmentCount = 6;
                        const int bodyStartY = std::max(0, bottomStartY - bodySegmentCount);

                        map.AddForegroundSprite(
                            bottomStartX,
                            bottomStartY,
                            bottomTileWidth,
                            bottomTileHeight,
                            warpPipeBottomResolved
                        );
                        for (int segment = 0; segment < bodySegmentCount; ++segment) {
                            const int segmentY = bodyStartY + segment;
                            if (segmentY < 0 || segmentY >= layerHeight) {
                                continue;
                            }
                            map.AddForegroundSprite(
                                forkedBodyMinX,
                                segmentY,
                                bodyTileWidth,
                                1,
                                pipeBodyResolved
                            );
                        }

                        const float entryX =
                            map.GetWorldLeft() + (static_cast<float>(forkedBottomMinX) - 1.5f) * map.GetTileSize();
                        map.SetTransitionPipeEntryX(entryX);
                        undergroundExitPipeVisualAdded = true;
                        handledUndergroundExitPipe = true;
                    } else if (!undergroundExitPipeVisualAdded &&
                        haveExitPipeSet &&
                        minX > static_cast<int>(width * 0.75f) &&
                        spanX >= 4) {
                        const int verticalLeftX = std::clamp(maxX - 1, 1, std::max(1, width - 2));
                        const int smallLeftX = std::max(0, verticalLeftX - 4);
                        const int mouthLeftX = smallLeftX + 2;
                        const int mouthRightX = verticalLeftX - 1;
                        const int mouthTopY = std::max(minY, maxY - 1);
                        const int mouthBottomY = maxY;

                        for (int pipeY = 0; pipeY <= maxY; ++pipeY) {
                            AddForegroundPipeTile(
                                verticalLeftX,
                                pipeY,
                                pipeY == 0 ? exitPipeTopLeft : exitPipeBodyLeft
                            );
                            AddForegroundPipeTile(
                                verticalLeftX + 1,
                                pipeY,
                                pipeY == 0 ? exitPipeTopRight : exitPipeBodyRight
                            );
                        }

                        AddForegroundPipeTile(smallLeftX, mouthTopY, exitMouthTopLeft);
                        AddForegroundPipeTile(smallLeftX + 1, mouthTopY, exitMouthTopRight);
                        AddForegroundPipeTile(smallLeftX, mouthBottomY, exitMouthBottomLeft);
                        AddForegroundPipeTile(smallLeftX + 1, mouthBottomY, exitMouthBottomRight);

                        for (int pipeX = mouthLeftX; pipeX <= mouthRightX; ++pipeX) {
                            const bool isRightEnd = pipeX == mouthRightX;
                            AddForegroundPipeTile(
                                pipeX,
                                mouthTopY,
                                isRightEnd ? exitMouthTopRight : exitMouthTopMiddle
                            );
                            AddForegroundPipeTile(
                                pipeX,
                                mouthBottomY,
                                isRightEnd ? exitMouthBottomRight : exitMouthBottomMiddle
                            );
                        }

                        const float entryX =
                            map.GetWorldLeft() + (static_cast<float>(smallLeftX) + 0.5f) * map.GetTileSize();
                        map.SetTransitionPipeEntryX(entryX);
                        for (int skipX = smallLeftX; skipX < width; ++skipX) {
                            for (int skipY = 0; skipY < layerHeight; ++skipY) {
                                if (pipeMask[skipX][skipY]) {
                                    visited[skipX][skipY] = 1;
                                }
                            }
                        }
                        undergroundExitPipeVisualAdded = true;
                        handledUndergroundExitPipe = true;
                    } else if (spanX >= 2) {
                        const float plantX =
                            map.GetWorldLeft() + (static_cast<float>(minX) + spanX * 0.5f) * map.GetTileSize();
                        const float pipeTopY =
                            (map.GetHeight() * map.GetTileSize()) / 2.0f - minY * map.GetTileSize();
                        venusSpawnPositions.push_back({ plantX, pipeTopY + 28.0f });
                    }
                }

                if (!handledUndergroundExitPipe) {
                    if (isUndergroundLevel &&
                        !pipeBodyResolved.empty() &&
                        fs::exists(pipeBodyResolved) &&
                        spanX >= 2) {
                        if (!pipeResolved.empty() && fs::exists(pipeResolved)) {
                            map.AddTileSprite(minX, minY, spanX, 1, Cell::Pipe, pipeResolved);
                        } else {
                            for (int cellX = minX; cellX <= maxX; ++cellX) {
                                const bool isLeftColumn = cellX == minX;
                                const std::string& pipePiece = isLeftColumn ? pipeTopLeft : pipeTopRight;
                                map.AddTile(cellX, minY, Cell::Pipe, pipePiece);
                            }
                        }

                        if (maxY > minY) {
                            map.AddTileSprite(minX, minY + 1, spanX, maxY - minY, Cell::Pipe, pipeBodyResolved);
                        }
                    } else {
                        for (const auto& [cellX, cellY] : cells) {
                            const bool isTopRow = cellY == minY;
                            const bool isLeftColumn = cellX == minX;
                            const std::string& pipePiece =
                                isTopRow
                                    ? (isLeftColumn ? pipeTopLeft : pipeTopRight)
                                    : (isLeftColumn ? pipeBottomLeft : pipeBottomRight);
                            map.AddTile(cellX, cellY, Cell::Pipe, pipePiece);
                        }
                    }
                }
            }
        }
    } else {
        if (!pipeResolved.empty() && fs::exists(pipeResolved)) {
            AddComponentSprites(pipeMask, false, Cell::Pipe, pipeResolved);
        }
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
    if (!woodResolved.empty() && fs::exists(woodResolved)) {
        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < layerHeight; ++y) {
                if (woodMask[x][y]) {
                    map.AddBackgroundTile(x, y, woodResolved);
                }
            }
        }
    }
    if (!treeLeftResolved.empty() && fs::exists(treeLeftResolved) &&
        !treeMiddleResolved.empty() && fs::exists(treeMiddleResolved) &&
        !treeRightResolved.empty() && fs::exists(treeRightResolved)) {
        for (int y = 0; y < layerHeight; ++y) {
            int x = 0;
            while (x < width) {
                if (!treeMask[x][y]) {
                    ++x;
                    continue;
                }

                int startX = x;
                while (x + 1 < width && treeMask[x + 1][y]) {
                    ++x;
                }
                const int endX = x;
                const int spanX = endX - startX + 1;

                if (spanX == 1) {
                    map.AddTile(startX, y, Cell::Wall, treeMiddleResolved);
                } else {
                    map.AddTile(startX, y, Cell::Wall, treeLeftResolved);
                    for (int fillX = startX + 1; fillX < endX; ++fillX) {
                        map.AddTile(fillX, y, Cell::Wall, treeMiddleResolved);
                    }
                    map.AddTile(endX, y, Cell::Wall, treeRightResolved);
                }
                ++x;
            }
        }
    }
    if (!castleResolved.empty() && fs::exists(castleResolved)) {
        AddMarkerScaledSprite(castleMask, castleResolved);
    }
    const bool haveForkedPipePieces =
        !isTransitionSceneSketch &&
        !pipeForkedResolved.empty() && fs::exists(pipeForkedResolved) &&
        haveExitPipeSet;
    auto AddForkedPipePieces = [&]() {
        Util::Image pipeImage(pipeForkedResolved);
        const glm::vec2 pipeImageSize = pipeImage.GetSize();
        if (pipeImageSize.x <= 0.0f || pipeImageSize.y <= 0.0f) {
            return false;
        }

        const int spriteTileWidth =
            std::max(2, static_cast<int>(std::round((pipeImageSize.x * 3.0f) / map.GetTileSize())));
        const int spriteTileHeight =
            std::max(3, static_cast<int>(std::round((pipeImageSize.y * 3.0f) / map.GetTileSize())));
        const int verticalSpanX = 2;

        std::vector<std::vector<char>> visited(width, std::vector<char>(layerHeight, 0));
        for (int sx = 0; sx < width; ++sx) {
            for (int sy = 0; sy < layerHeight; ++sy) {
                if (!pipeForkedMask[sx][sy] || visited[sx][sy]) continue;

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
                        const int nx = cx + dx[i];
                        const int ny = cy + dy[i];
                        if (nx < 0 || nx >= width || ny < 0 || ny >= layerHeight) continue;
                        if (visited[nx][ny] || !pipeForkedMask[nx][ny]) continue;
                        visited[nx][ny] = 1;
                        q.push({nx, ny});
                    }
                }

                const int markerCenterX = (minX + maxX) / 2;
                const int markerBaseY = maxY;
                const int startX =
                    std::clamp(markerCenterX - spriteTileWidth / 2, 0, std::max(0, width - spriteTileWidth));
                const int startY =
                    std::clamp(markerBaseY - spriteTileHeight + 1, 0, std::max(0, layerHeight - spriteTileHeight));
                const int endY = std::min(layerHeight - 1, startY + spriteTileHeight - 1);
                const int verticalLeftX =
                    std::clamp(startX + spriteTileWidth - verticalSpanX, 0, std::max(0, width - verticalSpanX));
                const int verticalRightX = verticalLeftX + 1;

                for (int pipeY = startY; pipeY <= endY; ++pipeY) {
                    map.AddForegroundSprite(
                        verticalLeftX,
                        pipeY,
                        1,
                        1,
                        pipeY == startY ? exitPipeTopLeft : exitPipeBodyLeft
                    );
                    map.AddForegroundSprite(
                        verticalRightX,
                        pipeY,
                        1,
                        1,
                        pipeY == startY ? exitPipeTopRight : exitPipeBodyRight
                    );
                }

                const int mouthBottomY = endY;
                const int mouthTopY = std::max(startY, mouthBottomY - 1);
                const int mouthLeftX = startX;
                const int mouthRightX = std::max(mouthLeftX + 1, verticalLeftX - 1);

                for (int pipeX = mouthLeftX; pipeX <= mouthRightX; ++pipeX) {
                    const bool isLeftEnd = pipeX == mouthLeftX;
                    const bool isRightEnd = pipeX == mouthRightX;
                    map.AddForegroundSprite(
                        pipeX,
                        mouthTopY,
                        1,
                        1,
                        isLeftEnd ? exitMouthTopLeft : (isRightEnd ? exitMouthTopRight : exitMouthTopMiddle)
                    );
                    map.AddForegroundSprite(
                        pipeX,
                        mouthBottomY,
                        1,
                        1,
                        isLeftEnd ? exitMouthBottomLeft : (isRightEnd ? exitMouthBottomRight : exitMouthBottomMiddle)
                    );
                }

                const float entryX =
                    map.GetWorldLeft() + (static_cast<float>(verticalLeftX) + 0.5f) * map.GetTileSize();
                map.SetTransitionPipeEntryX(entryX);
            }
        }

        return true;
    };
    if (haveForkedPipePieces) {
        AddForkedPipePieces();
    } else if (!pipeForkedResolved.empty() && fs::exists(pipeForkedResolved)) {
        AddMarkerScaledSprite(
            pipeForkedMask,
            pipeForkedResolved,
            true,
            [&](int, int, int, int, int markerCenterX, int) {
                const float entryX =
                    map.GetWorldLeft() + markerCenterX * map.GetTileSize() + map.GetTileSize() * 0.5f;
                map.SetTransitionPipeEntryX(entryX);
            }
        );
    }

    {
        std::vector<std::vector<char>> visited(width, std::vector<char>(layerHeight, 0));
        for (int sx = 0; sx < width; ++sx) {
            for (int sy = 0; sy < layerHeight; ++sy) {
                if (!movingPlatformMask[sx][sy] || visited[sx][sy]) continue;

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
                        const int nx = cx + dx[i];
                        const int ny = cy + dy[i];
                        if (nx < 0 || nx >= width || ny < 0 || ny >= layerHeight) continue;
                        if (visited[nx][ny] || !movingPlatformMask[nx][ny]) continue;
                        visited[nx][ny] = 1;
                        q.push({nx, ny});
                    }
                }

                const int spanX = maxX - minX + 1;
                const int spanY = maxY - minY + 1;
                const int defaultTravelTiles = 4;
                const int topGridY =
                    (spanY > 1) ? minY : std::max(0, maxY - (defaultTravelTiles - 1));
                const int bottomGridY = maxY;
                map.AddMovingPlatform(minX, topGridY, bottomGridY, spanX, "platform.png");
            }
        }
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

            const std::string flagstickPath = MapManager::ResolveBackgroundPath("flagstick.png");
            const std::string dotPath = MapManager::ResolveBackgroundPath("dot.png");
            const std::string flagPath = MapManager::ResolveBackgroundPath("flag.png");
            const std::string hardBlockPath = MapManager::ResolveTilePath(
                Cell::Wall,
                isUndergroundLevel ? "SMB_Underground_Hard_Block.png" : "HardBlock.png"
            );
            const int poleBottomY = maxY;
            const int maxBaseY = layerHeight - 1;
            const int baseY = std::min(maxBaseY, poleBottomY + 1);
            const int flagX = std::max(0, minX - 1);
            const int flagY = std::min(poleBottomY, minY + 1);
            const int clearTopY = std::max(0, minY - 3);

            for (int clearX = flagX; clearX <= maxX; ++clearX) {
                for (int clearY = clearTopY; clearY <= baseY; ++clearY) {
                    map.ClearTile(clearX, clearY);
                }
            }

            for (int poleY = minY + 1; poleY <= poleBottomY; ++poleY) {
                map.AddBackgroundTile(minX, poleY, flagstickPath);
            }
            map.AddBackgroundTile(minX, minY, dotPath);
            map.AddTile(minX, baseY, Cell::Wall, hardBlockPath);
            map.ConfigureGoalVisuals(minX, flagX, flagY, poleBottomY, baseY, flagPath);
        }
    }

    if (enemy_spawns != nullptr) {
        if (isUndergroundLevel && !venusSpawnPositions.empty()) {
            std::sort(
                venusSpawnPositions.begin(),
                venusSpawnPositions.end(),
                [](const glm::vec2& lhs, const glm::vec2& rhs) { return lhs.x < rhs.x; }
            );
            const std::size_t plantCount = std::min<std::size_t>(2, venusSpawnPositions.size());
            for (std::size_t i = 0; i < plantCount; ++i) {
                enemy_spawns->push_back({ venusSpawnPositions[i], EnemyKind::Venus });
            }
        }

        const auto appendEnemySpawns = [&](const std::vector<std::vector<char>>& mask, EnemyKind kind) {
            std::vector<std::vector<char>> visited(width, std::vector<char>(layerHeight, 0));
            const float halfHeight =
                (kind == EnemyKind::GreenKoopa || kind == EnemyKind::RedKoopa) ? 36.0f : 24.0f;
            for (int sx = 0; sx < width; ++sx) {
                for (int sy = 0; sy < layerHeight; ++sy) {
                    if (!mask[sx][sy] || visited[sx][sy]) continue;

                    int minX = sx;
                    int maxX = sx;
                    int minY = sy;
                    std::queue<std::pair<int, int>> q;
                    q.push({sx, sy});
                    visited[sx][sy] = 1;

                    while (!q.empty()) {
                        auto [cx, cy] = q.front();
                        q.pop();
                        minX = std::min(minX, cx);
                        maxX = std::max(maxX, cx);
                        minY = std::min(minY, cy);

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

                    const int centerX = (minX + maxX) / 2;
                    enemy_spawns->push_back({
                        ComputeGroundedSpawnPosition(map, centerX, minY, halfHeight),
                        kind
                    });
                }
            }
        };

        appendEnemySpawns(goombaMask, EnemyKind::Goomba);
        appendEnemySpawns(koopaMask, EnemyKind::GreenKoopa);
        appendEnemySpawns(redKoopaMask, EnemyKind::RedKoopa);
    }

    std::string hardBlockPath = MapManager::ResolveTilePath(
        Cell::Brick,
        isUndergroundLevel ? "SMB_Underground_Hard_Block.png" : "HardBlock.png"
    );
    bool haveHardBlockAsset = !hardBlockPath.empty() && fs::exists(hardBlockPath);
    if (haveHardBlockAsset && !isUndergroundLevel) {
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
                }
            }
        }
    }

    bool spawnFound = false;
    if (spawnMarkerFound) {
        mario.m_Transform.translation = shouldUseMarkerSpawn
            ? ComputeMarkerSpawnPosition(map, marioSpawnX, marioSpawnY)
            : ComputeGroundedSpawnPosition(
                map,
                marioSpawnX,
                marioSpawnY,
                mario.GetHalfExtents().y
            );
        spawnFound = true;
    }

    SDL_FreeSurface(surface);

    if (!spawnFound) {
        LOG_ERROR("No red Mario spawn pixel found in entity layer.");
    }

    return spawnFound;
}

