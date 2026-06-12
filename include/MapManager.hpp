#ifndef MAP_MANAGER_HPP
#define MAP_MANAGER_HPP

#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <set>
#include <queue>
#include <glm/vec2.hpp> // for glm::vec2

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"
#include "Animation.hpp"
#include "config.hpp"

enum class Cell { Empty, Wall, Brick, QuestionBlock, Pipe, Coin };
enum class LootType { RedMushroom, GreenMushroom, FireFlower, Coin, Star, ProgressivePowerUp };

class MapManager {
public:
    struct CollisionBox {
        glm::vec2 center = { 0.0f, 0.0f };
        glm::vec2 halfExtents = { 0.0f, 0.0f };
    };

    enum class MovingPlatformState {
        MovingUp,
        MovingDown,
        WaitingTop,
        WaitingBottom
    };

    enum class MovingPlatformMotion {
        Vertical,
        Horizontal
    };

    enum class MovingPlatformCycle {
        Bounce,
        WrapDown,
        WrapUp
    };

    struct MovingPlatformSnapshot {
        glm::vec2 center = { 0.0f, 0.0f };
        glm::vec2 halfExtents = { 0.0f, 0.0f };
        glm::vec2 delta = { 0.0f, 0.0f };
        bool wrappedThisFrame = false;
    };

private:
    bool m_IsUndergroundTheme = false;
    std::vector<std::vector<Cell>> m_Map;
    std::vector<std::vector<std::shared_ptr<Util::GameObject>>> m_TileObjects;
    std::vector<std::vector<std::shared_ptr<Util::GameObject>>> m_BackgroundTileObjects;

    // solid / gameplay objects
    std::vector<std::shared_ptr<Util::GameObject>> m_Objects;

    // decorative background objects
    std::vector<std::shared_ptr<Util::GameObject>> m_BackgroundObjects;

    // decorative foreground objects that should draw in front of Mario
    std::vector<std::shared_ptr<Util::GameObject>> m_ForegroundObjects;

    struct AnimatedTile {
        std::shared_ptr<Util::GameObject> object;
        std::shared_ptr<Util::Image> image;
        Animation animation;
        int gridX = 0;
        int gridY = 0;
    };
    struct MovingPlatform {
        std::shared_ptr<Util::GameObject> object;
        std::shared_ptr<Util::Image> image;
        MovingPlatformState state = MovingPlatformState::MovingUp;
        MovingPlatformMotion motion = MovingPlatformMotion::Vertical;
        glm::vec2 halfExtents = { 0.0f, 0.0f };
        glm::vec2 previousCenter = { 0.0f, 0.0f };
        glm::vec2 delta = { 0.0f, 0.0f };
        bool wrappedThisFrame = false;
        float moveSpeed = 72.0f;
        float topLimit = 0.0f;
        float bottomLimit = 0.0f;
        float leftLimit = 0.0f;
        float rightLimit = 0.0f;
        float waitTime = 0.18f;
        float waitTimer = 0.0f;
        MovingPlatformCycle cycle = MovingPlatformCycle::Bounce;
    };
    std::vector<AnimatedTile> m_AnimatedTiles;
    std::vector<MovingPlatform> m_MovingPlatforms;
    std::vector<std::vector<bool>> m_QuestionBlockUsed;
    std::vector<std::vector<bool>> m_HiddenQuestionBlocks;
    std::vector<std::vector<LootType>> m_QuestionBlockLoot;
    std::vector<std::vector<int>> m_QuestionBlockRemainingHits;
    struct SpawnEvent {
        LootType type;
        glm::vec2 position;
    };
    std::queue<SpawnEvent> m_SpawnEvents;
    struct BrickBreakEvent {
        glm::vec2 position;
    };
    std::queue<BrickBreakEvent> m_BrickBreakEvents;
    struct CoinCollectEvent {
        glm::vec2 position;
    };
    std::queue<CoinCollectEvent> m_CoinCollectEvents;
    bool m_HasGoal = false;
    float m_GoalX = 0.0f;
    float m_GoalGroundY = 0.0f;
    float m_FlagTopY = 0.0f;
    float m_FlagBottomY = 0.0f;
    float m_FlagX = 0.0f;
    float m_GoalSlideStartY = 0.0f;
    std::shared_ptr<Util::GameObject> m_GoalFlagObject;
    std::shared_ptr<Util::Image> m_GoalFlagImage;
    bool m_HasTransitionPipe = false;
    float m_TransitionPipeEntryX = 0.0f;
    int m_Width = 0;
    int m_Height = 0;

    const float TILE_SIZE = 48.0f;

public:
    MapManager() = default;

    void SetUndergroundTheme(bool isUnderground) { m_IsUndergroundTheme = isUnderground; }
    bool IsUndergroundTheme() const { return m_IsUndergroundTheme; }

    static bool IsSolidCell(Cell cell) {
        return cell == Cell::Wall ||
               cell == Cell::Brick ||
               cell == Cell::QuestionBlock ||
               cell == Cell::Pipe;
    }

    // helper: resource root via compile-time define or fallback
    static std::filesystem::path ResourceRoot() {
#ifdef RESOURCE_DIR
        return std::filesystem::path(RESOURCE_DIR);
#else
        return std::filesystem::path("Resources");
#endif
    }

    static std::string FindImageByFilename(const std::filesystem::path& baseDir,
                                           const std::string& filename) {
        namespace fs = std::filesystem;
        if (filename.empty() || !fs::exists(baseDir)) return std::string();

        fs::path targetName = fs::path(filename).filename();

        fs::path direct = baseDir / targetName;
        if (fs::exists(direct)) return direct.string();

        for (const auto& subdir : { fs::path("Tiles"), fs::path("Background"), fs::path("character"), fs::path("Character"), fs::path("Pipes") }) {
            fs::path candidate = baseDir / subdir / targetName;
            if (fs::exists(candidate)) return candidate.string();
        }

        for (auto& entry : fs::recursive_directory_iterator(baseDir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().filename() == targetName) {
                return entry.path().string();
            }
        }

        return std::string();
    }

    static std::string FindImageByKeywords(const std::filesystem::path& baseDir,
                                           const std::vector<std::string>& keywords) {
        namespace fs = std::filesystem;
        if (!fs::exists(baseDir)) return std::string();

        for (auto& entry : fs::recursive_directory_iterator(baseDir)) {
            if (!entry.is_regular_file()) continue;

            std::string name = entry.path().filename().string();
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            for (const auto& keyword : keywords) {
                std::string lowered = keyword;
                std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                if (name.find(lowered) != std::string::npos) {
                    return entry.path().string();
                }
            }
        }

        return std::string();
    }

    // helper: ensure path exists or try sensible fallbacks
    static std::string ResolveTilePath(Cell type, const std::string &requested) {
        namespace fs = std::filesystem;
        fs::path req = requested;
        if (fs::exists(req)) return requested;

        fs::path root = ResourceRoot();
        fs::path imageRoot = root / "image";

        if (!req.filename().empty()) {
            std::string found = FindImageByFilename(imageRoot, req.filename().string());
            if (!found.empty()) {
                return found;
            }
        }

        // fallback mapping by Cell type
        std::string fallback;
        switch (type) {
            case Cell::Wall:
                fallback = FindImageByFilename(imageRoot, "Ground.png");
                break;
            case Cell::Brick:
                fallback = FindImageByFilename(imageRoot, "Brick.png");
                break;
            case Cell::QuestionBlock:
                fallback = FindImageByFilename(imageRoot, "Question.png");
                break;
            case Cell::Pipe:
                fallback = FindImageByFilename(imageRoot, "Pipe.png");
                break;
            case Cell::Coin:
                fallback = FindImageByFilename(imageRoot, "coin1.png");
                break;
            default:
                break;
        }
        if (!fallback.empty()) return fallback;

        // last resort: try any tile in Tiles folder
        if (fs::exists(imageRoot / "Tiles")) {
            for (auto &entry : fs::directory_iterator(imageRoot / "Tiles")) {
                if (!entry.is_directory()) {
                    LOG_WARN("Requested tile '{}' not found; using '{}'", requested, entry.path().string());
                    return entry.path().string();
                }
            }
        }

        LOG_WARN("Requested tile '{}' not found and no fallback available; using original path", requested);
        return requested;
    }

    static std::string ResolveBackgroundPath(const std::string &requested) {
        namespace fs = std::filesystem;
        fs::path req = requested;
        if (fs::exists(req)) return requested;

        fs::path root = ResourceRoot();
        fs::path imageRoot = root / "image";

        if (!req.filename().empty()) {
            std::string found = FindImageByFilename(imageRoot, req.filename().string());
            if (!found.empty()) {
                return found;
            }
        }

        // try keyword-based fallback
        std::string name = req.filename().string();
        if (name.find("Cloud") != std::string::npos || name.find("cloud") != std::string::npos) {
            std::string found = FindImageByKeywords(imageRoot, { "cloud", "mountain" });
            if (!found.empty()) return found;
        }
        if (name.find("Grass") != std::string::npos || name.find("grass") != std::string::npos) {
            std::string found = FindImageByKeywords(imageRoot, { "grass", "mountain" });
            if (!found.empty()) return found;
        }
        if (name.find("Hill") != std::string::npos || name.find("hill") != std::string::npos) {
            std::string found = FindImageByKeywords(imageRoot, { "hill", "mountain" });
            if (!found.empty()) return found;
        }
        if (name.find("Flag") != std::string::npos || name.find("flag") != std::string::npos) {
            std::string found = FindImageByKeywords(imageRoot, { "flag", "castle", "pipe" });
            if (!found.empty()) return found;
        }

        std::string fallback = FindImageByKeywords(imageRoot, { "mountain", "castle", "background", "cloud" });
        if (!fallback.empty()) {
            LOG_WARN("Requested background '{}' not found; using '{}'", requested, fallback);
            return fallback;
        }

        LOG_WARN("Requested background '{}' not found and no fallback available; using original path", requested);
        return requested;
    }

    void SetMapSize(int width, int height) {
        m_Width = width;
        m_Height = height;
        m_IsUndergroundTheme = false;

        m_Map.assign(width, std::vector<Cell>(height, Cell::Empty));
        m_TileObjects.assign(width, std::vector<std::shared_ptr<Util::GameObject>>(height, nullptr));
        m_BackgroundTileObjects.assign(width, std::vector<std::shared_ptr<Util::GameObject>>(height, nullptr));
        m_QuestionBlockUsed.assign(width, std::vector<bool>(height, false));
        m_HiddenQuestionBlocks.assign(width, std::vector<bool>(height, false));
        m_QuestionBlockLoot.assign(width, std::vector<LootType>(height, LootType::Coin));
        m_QuestionBlockRemainingHits.assign(width, std::vector<int>(height, 1));
        m_Objects.clear();
        m_BackgroundObjects.clear();
        m_ForegroundObjects.clear();
        m_AnimatedTiles.clear();
        m_MovingPlatforms.clear();
        std::queue<SpawnEvent> empty;
        std::swap(m_SpawnEvents, empty);
        std::queue<BrickBreakEvent> emptyBreaks;
        std::swap(m_BrickBreakEvents, emptyBreaks);
        m_HasGoal = false;
        m_GoalX = 0.0f;
        m_GoalGroundY = 0.0f;
        m_FlagTopY = 0.0f;
        m_FlagBottomY = 0.0f;
        m_FlagX = 0.0f;
        m_GoalSlideStartY = 0.0f;
        m_GoalFlagObject = nullptr;
        m_GoalFlagImage = nullptr;
        m_HasTransitionPipe = false;
        m_TransitionPipeEntryX = 0.0f;
    }

    void AddTileSprite(int gridX, int gridY, int tileSpanX, int tileSpanY,
                       Cell type, const std::string& texturePath) {
        if (gridX < 0 || gridX >= m_Width || gridY < 0 || gridY >= m_Height)
            return;
        int endX = std::min(m_Width, gridX + std::max(tileSpanX, 1));
        int endY = std::min(m_Height, gridY + std::max(tileSpanY, 1));

        std::set<std::shared_ptr<Util::GameObject>> toRemove;
        for (int x = gridX; x < endX; ++x) {
            for (int y = gridY; y < endY; ++y) {
                if (m_TileObjects[x][y] != nullptr) {
                    toRemove.insert(m_TileObjects[x][y]);
                }
            }
        }
        for (const auto& existing : toRemove) {
            m_Objects.erase(std::remove(m_Objects.begin(), m_Objects.end(), existing), m_Objects.end());
        }

        // resolve texture path (try fallbacks if missing)
        std::string resolvedPath = ResolveTilePath(type, texturePath);

        auto obj = std::make_shared<Util::GameObject>();

        auto img = std::make_shared<Util::Image>(resolvedPath);
        obj->SetDrawable(img);

        float xPos = -(m_Width * TILE_SIZE) / 2.0f + (gridX * TILE_SIZE) + (tileSpanX * TILE_SIZE) / 2.0f;
        float yPos = (m_Height * TILE_SIZE) / 2.0f - (gridY * TILE_SIZE) - (tileSpanY * TILE_SIZE) / 2.0f;

        obj->m_Transform.translation = { xPos, yPos };

        glm::vec2 imgSize = img->GetSize();
        glm::vec2 scale = {1.0f, 1.0f};
        if (imgSize.x > 0.0f && imgSize.y > 0.0f) {
            if (type == Cell::Coin) {
                const float targetWidth = tileSpanX * TILE_SIZE * 0.8f;
                const float targetHeight = tileSpanY * TILE_SIZE * 0.875f;
                const float uniformScale = std::min(targetWidth / imgSize.x, targetHeight / imgSize.y);
                scale = { uniformScale, uniformScale };
            } else {
                scale.x = (tileSpanX * TILE_SIZE) / imgSize.x;
                scale.y = (tileSpanY * TILE_SIZE) / imgSize.y;
            }
        }
        obj->m_Transform.scale = scale;

        obj->SetZIndex(1.0f);

        for (int x = gridX; x < endX; ++x) {
            for (int y = gridY; y < endY; ++y) {
                m_Map[x][y] = type;
                m_TileObjects[x][y] = obj;
            }
        }
        m_Objects.push_back(obj);

        if (type == Cell::QuestionBlock || type == Cell::Coin) {
            m_AnimatedTiles.erase(
                std::remove_if(m_AnimatedTiles.begin(), m_AnimatedTiles.end(),
                               [&](const AnimatedTile& tile) { return tile.object == obj; }),
                m_AnimatedTiles.end()
            );

            std::vector<std::string> frames;
            float frameDuration = 0.12f;
            if (type == Cell::QuestionBlock) {
                frames = {
                    ResolveTilePath(Cell::QuestionBlock, m_IsUndergroundTheme ? "ug_question1.png" : "Question1.png"),
                    ResolveTilePath(Cell::QuestionBlock, m_IsUndergroundTheme ? "ug_question2.png" : "Question2.png"),
                    ResolveTilePath(Cell::QuestionBlock, m_IsUndergroundTheme ? "ug_question3.png" : "Question3.png")
                };
            } else {
                frames = {
                    ResolveTilePath(Cell::Coin, m_IsUndergroundTheme ? "ug_coin1.png" : "coin1.png"),
                    ResolveTilePath(Cell::Coin, m_IsUndergroundTheme ? "ug_coin2.png" : "coin2.png"),
                    ResolveTilePath(Cell::Coin, m_IsUndergroundTheme ? "ug_coin3.png" : "coin3.png"),
                    ResolveTilePath(Cell::Coin, m_IsUndergroundTheme ? "ug_coin4.png" : "coin4.png")
                };
                frameDuration = 0.09f;
            }
            m_AnimatedTiles.push_back({ obj, img, Animation(frames, frameDuration), gridX, gridY });
        }
    }

    void AddTile(int gridX, int gridY, Cell type, const std::string& texturePath) {
        AddTileSprite(gridX, gridY, 1, 1, type, texturePath);
    }

    void AddMovingPlatform(int gridX,
                           int topGridY,
                           int bottomGridY,
                           int tileSpanX,
                           const std::string& texturePath,
                           float moveSpeed = 72.0f,
                           float waitTime = 0.18f,
                           MovingPlatformMotion motion = MovingPlatformMotion::Vertical,
                           int horizontalTravelTiles = 6,
                           int horizontalLeftGridX = -1,
                           int horizontalRightGridX = -1,
                           bool startAtRight = false,
                           float initialWaitTime = 0.0f,
                           MovingPlatformCycle cycle = MovingPlatformCycle::Bounce,
                           float initialCycleOffset = 0.0f) {
        if (m_Width <= 0 || m_Height <= 0) return;

        const int clampedGridX = std::clamp(gridX, 0, std::max(0, m_Width - 1));
        const int clampedTopGridY = std::clamp(topGridY, 0, std::max(0, m_Height - 1));
        const int clampedBottomGridY = std::clamp(bottomGridY, clampedTopGridY, std::max(0, m_Height - 1));
        const int spanX = std::max(1, tileSpanX);
        const std::string resolvedPath = ResolveBackgroundPath(texturePath);

        auto object = std::make_shared<Util::GameObject>();
        auto image = std::make_shared<Util::Image>(resolvedPath);
        object->SetDrawable(image);

        const float targetWidth = spanX * TILE_SIZE;
        const float targetHeight = TILE_SIZE * 0.5f;
        const glm::vec2 imageSize = image->GetSize();
        glm::vec2 scale = { 1.0f, 1.0f };
        if (imageSize.x > 0.0f && imageSize.y > 0.0f) {
            scale.x = targetWidth / imageSize.x;
            scale.y = targetHeight / imageSize.y;
        }

        const float centerX =
            -(m_Width * TILE_SIZE) / 2.0f + clampedGridX * TILE_SIZE + targetWidth * 0.5f;
        const auto gridToCenterY = [&](int gridY) {
            return (m_Height * TILE_SIZE) / 2.0f - gridY * TILE_SIZE - TILE_SIZE * 0.5f;
        };
        const float topCenterY = gridToCenterY(clampedTopGridY);
        const float bottomCenterY = gridToCenterY(clampedBottomGridY);

        object->m_Transform.translation = {
            centerX,
            cycle != MovingPlatformCycle::Bounce ? topCenterY : bottomCenterY
        };
        object->m_Transform.scale = scale;
        object->SetZIndex(8.0f);

        MovingPlatform platform;
        platform.object = object;
        platform.image = image;
        platform.state = MovingPlatformState::MovingUp;
        platform.motion = motion;
        platform.halfExtents = { targetWidth * 0.5f, targetHeight * 0.5f };
        platform.previousCenter = object->m_Transform.translation;
        platform.delta = { 0.0f, 0.0f };
        platform.moveSpeed = std::max(1.0f, moveSpeed);
        platform.topLimit = std::max(topCenterY, bottomCenterY);
        platform.bottomLimit = std::max(std::min(topCenterY, bottomCenterY),
                                        -(m_Height * TILE_SIZE) / 2.0f + platform.halfExtents.y);
        const float worldLeft = -(m_Width * TILE_SIZE) / 2.0f + platform.halfExtents.x;
        const float worldRight = (m_Width * TILE_SIZE) / 2.0f - platform.halfExtents.x;
        if (horizontalLeftGridX >= 0 && horizontalRightGridX >= horizontalLeftGridX) {
            const int clampedLeftGridX = std::clamp(horizontalLeftGridX, 0, std::max(0, m_Width - spanX));
            const int clampedRightGridX = std::clamp(horizontalRightGridX, clampedLeftGridX, std::max(0, m_Width - spanX));
            platform.leftLimit =
                -(m_Width * TILE_SIZE) / 2.0f + clampedLeftGridX * TILE_SIZE + platform.halfExtents.x;
            platform.rightLimit =
                -(m_Width * TILE_SIZE) / 2.0f + clampedRightGridX * TILE_SIZE + platform.halfExtents.x;
        } else {
            const float horizontalTravel = std::max(1, horizontalTravelTiles) * TILE_SIZE;
            platform.leftLimit = std::max(worldLeft, centerX - horizontalTravel);
            platform.rightLimit = std::min(worldRight, centerX + horizontalTravel);
        }
        if (platform.motion == MovingPlatformMotion::Horizontal) {
            platform.state = startAtRight ? MovingPlatformState::MovingDown : MovingPlatformState::MovingUp;
        }
        platform.previousCenter = object->m_Transform.translation;
        platform.waitTime = std::max(0.0f, waitTime);
        platform.waitTimer = std::max(0.0f, initialWaitTime);
        platform.cycle = cycle;
        if (platform.motion == MovingPlatformMotion::Vertical &&
            platform.cycle != MovingPlatformCycle::Bounce) {
            platform.topLimit = (m_Height * TILE_SIZE) / 2.0f + platform.halfExtents.y;
            platform.bottomLimit = -(m_Height * TILE_SIZE) / 2.0f - platform.halfExtents.y;
            platform.state = platform.cycle == MovingPlatformCycle::WrapDown
                ? MovingPlatformState::MovingDown
                : MovingPlatformState::MovingUp;
            const float wrappedOffset = initialCycleOffset - std::floor(initialCycleOffset);
            if (wrappedOffset > 0.0f) {
                const float travelDistance = platform.topLimit - platform.bottomLimit;
                object->m_Transform.translation.y =
                    platform.cycle == MovingPlatformCycle::WrapDown
                        ? platform.topLimit - travelDistance * wrappedOffset
                        : platform.bottomLimit + travelDistance * wrappedOffset;
            }
            platform.waitTime = 0.0f;
            platform.waitTimer = 0.0f;
        }
        if (platform.waitTimer > 0.0f) {
            platform.state = startAtRight ? MovingPlatformState::WaitingTop
                                          : MovingPlatformState::WaitingBottom;
        }

        m_MovingPlatforms.push_back(platform);
        m_Objects.push_back(object);
    }

    std::vector<MovingPlatformSnapshot> GetMovingPlatformSnapshots() const {
        std::vector<MovingPlatformSnapshot> snapshots;
        snapshots.reserve(m_MovingPlatforms.size());
        for (const auto& platform : m_MovingPlatforms) {
            snapshots.push_back({
                platform.object->m_Transform.translation,
                platform.halfExtents,
                platform.delta,
                platform.wrappedThisFrame
            });
        }
        return snapshots;
    }

    glm::vec2 GetCarryDelta(const glm::vec2& center,
                            const glm::vec2& halfExtents,
                            float tolerance = 4.0f) const {
        const float actorBottom = center.y - halfExtents.y;
        const float actorLeft = center.x - halfExtents.x;
        const float actorRight = center.x + halfExtents.x;

        for (const auto& platform : m_MovingPlatforms) {
            if (platform.wrappedThisFrame) continue;
            const glm::vec2 platformCenter = platform.object->m_Transform.translation;
            const float platformTop = platformCenter.y + platform.halfExtents.y;
            const float platformLeft = platformCenter.x - platform.halfExtents.x;
            const float platformRight = platformCenter.x + platform.halfExtents.x;
            const bool overlapX = actorRight > platformLeft + 2.0f && actorLeft < platformRight - 2.0f;
            if (!overlapX) continue;
            if (std::abs(actorBottom - platformTop) <= tolerance) {
                return platform.delta;
            }
        }

        return { 0.0f, 0.0f };
    }

    void SetQuestionBlockLoot(int x, int y, LootType lootType) {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;
        if (m_Map[x][y] != Cell::QuestionBlock) return;
        m_QuestionBlockLoot[x][y] = lootType;
    }

    void SetQuestionBlockHidden(int x, int y, bool hidden) {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;
        if (m_Map[x][y] != Cell::QuestionBlock) return;

        m_HiddenQuestionBlocks[x][y] = hidden;
        if (m_TileObjects[x][y] != nullptr) {
            m_TileObjects[x][y]->SetVisible(!hidden);
        }

        if (hidden) {
            m_AnimatedTiles.erase(
                std::remove_if(m_AnimatedTiles.begin(), m_AnimatedTiles.end(),
                               [&](const AnimatedTile& tile) { return tile.gridX == x && tile.gridY == y; }),
                m_AnimatedTiles.end()
            );
        }
    }

    bool IsQuestionBlockHidden(int x, int y) const {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return false;
        return m_Map[x][y] == Cell::QuestionBlock && m_HiddenQuestionBlocks[x][y];
    }

    void SetQuestionBlockHitCount(int x, int y, int hitCount) {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;
        if (m_Map[x][y] != Cell::QuestionBlock) return;
        m_QuestionBlockRemainingHits[x][y] = std::max(1, hitCount);
    }

    void SetQuestionBlockStaticTexture(int x, int y, const std::string& texturePath) {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;
        if (m_Map[x][y] != Cell::QuestionBlock) return;

        const std::string resolvedPath = ResolveTilePath(Cell::Brick, texturePath);
        if (m_TileObjects[x][y] != nullptr) {
            m_TileObjects[x][y]->SetDrawable(std::make_shared<Util::Image>(resolvedPath));
        }

        m_AnimatedTiles.erase(
            std::remove_if(m_AnimatedTiles.begin(), m_AnimatedTiles.end(),
                           [&](const AnimatedTile& tile) { return tile.gridX == x && tile.gridY == y; }),
            m_AnimatedTiles.end()
        );
    }

    void AddBackgroundSprite(int gridX, int gridY, int tileSpanX, int tileSpanY,
                             const std::string& texturePath) {
        if (gridX < 0 || gridX >= m_Width || gridY < 0 || gridY >= m_Height)
            return;
        int endX = std::min(m_Width, gridX + std::max(tileSpanX, 1));
        int endY = std::min(m_Height, gridY + std::max(tileSpanY, 1));

        std::set<std::shared_ptr<Util::GameObject>> toRemove;
        for (int x = gridX; x < endX; ++x) {
            for (int y = gridY; y < endY; ++y) {
                if (m_BackgroundTileObjects[x][y] != nullptr) {
                    toRemove.insert(m_BackgroundTileObjects[x][y]);
                }
            }
        }
        for (const auto& existing : toRemove) {
            m_BackgroundObjects.erase(
                std::remove(m_BackgroundObjects.begin(), m_BackgroundObjects.end(), existing),
                m_BackgroundObjects.end()
            );
        }

        // resolve background path (try fallbacks if missing)
        std::string resolvedPath = ResolveBackgroundPath(texturePath);

        auto obj = std::make_shared<Util::GameObject>();

        auto img = std::make_shared<Util::Image>(resolvedPath);
        obj->SetDrawable(img);

        float xPos = -(m_Width * TILE_SIZE) / 2.0f + (gridX * TILE_SIZE) + (tileSpanX * TILE_SIZE) / 2.0f;
        float yPos = (m_Height * TILE_SIZE) / 2.0f - (gridY * TILE_SIZE) - (tileSpanY * TILE_SIZE) / 2.0f;

        glm::vec2 imgSize = img->GetSize();
        if (imgSize.x > 0.0f && imgSize.y > 0.0f) {
            const float targetWidth = tileSpanX * TILE_SIZE;
            const float targetHeight = tileSpanY * TILE_SIZE;
            const float uniformScale = std::min(targetWidth / imgSize.x, targetHeight / imgSize.y);
            const float actualWidth = imgSize.x * uniformScale;
            const float actualHeight = imgSize.y * uniformScale;
            obj->m_Transform.scale = { uniformScale, uniformScale };
            obj->m_Transform.translation = {
                xPos,
                yPos - (targetHeight - actualHeight) / 2.0f
            };
        } else {
            obj->m_Transform.scale = { 1.0f, 1.0f };
            obj->m_Transform.translation = { xPos, yPos };
        }

        obj->SetZIndex(-10.0f);

        for (int x = gridX; x < endX; ++x) {
            for (int y = gridY; y < endY; ++y) {
                m_BackgroundTileObjects[x][y] = obj;
            }
        }
        m_BackgroundObjects.push_back(obj);
    }

    void AddBackgroundTile(int gridX, int gridY, const std::string& texturePath) {
        AddBackgroundSprite(gridX, gridY, 1, 1, texturePath);
    }

    void AddForegroundSprite(int gridX, int gridY, int tileSpanX, int tileSpanY,
                             const std::string& texturePath) {
        if (gridX < 0 || gridX >= m_Width || gridY < 0 || gridY >= m_Height) {
            return;
        }

        std::string resolvedPath = ResolveBackgroundPath(texturePath);

        auto obj = std::make_shared<Util::GameObject>();
        auto img = std::make_shared<Util::Image>(resolvedPath);
        obj->SetDrawable(img);

        const float xPos = -(m_Width * TILE_SIZE) / 2.0f + (gridX * TILE_SIZE) + (tileSpanX * TILE_SIZE) / 2.0f;
        const float yPos = (m_Height * TILE_SIZE) / 2.0f - (gridY * TILE_SIZE) - (tileSpanY * TILE_SIZE) / 2.0f;

        const glm::vec2 imgSize = img->GetSize();
        if (imgSize.x > 0.0f && imgSize.y > 0.0f) {
            const float targetWidth = tileSpanX * TILE_SIZE;
            const float targetHeight = tileSpanY * TILE_SIZE;
            const float uniformScale = std::min(targetWidth / imgSize.x, targetHeight / imgSize.y);
            const float actualWidth = imgSize.x * uniformScale;
            const float actualHeight = imgSize.y * uniformScale;
            obj->m_Transform.scale = { uniformScale, uniformScale };
            obj->m_Transform.translation = {
                xPos,
                yPos - (targetHeight - actualHeight) / 2.0f
            };
        } else {
            obj->m_Transform.scale = { 1.0f, 1.0f };
            obj->m_Transform.translation = { xPos, yPos };
        }

        obj->SetZIndex(40.0f);
        m_ForegroundObjects.push_back(obj);
    }

    Cell GetCell(int x, int y) const {
        if (x >= 0 && x < m_Width && y >= 0 && y < m_Height) {
            return m_Map[x][y];
        }
        // return Empty for out-of-range requests (safer for callers)
        return Cell::Empty;
    }

    bool IsSolidAt(int x, int y) const {
        const Cell cell = GetCell(x, y);
        if (cell == Cell::QuestionBlock && IsQuestionBlockHidden(x, y)) {
            return false;
        }
        return IsSolidCell(cell);
    }

    bool ClearTile(int x, int y) {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return false;
        if (m_Map[x][y] == Cell::Empty) return false;

        auto obj = m_TileObjects[x][y];
        if (obj != nullptr) {
            m_Objects.erase(std::remove(m_Objects.begin(), m_Objects.end(), obj), m_Objects.end());
            m_AnimatedTiles.erase(
                std::remove_if(m_AnimatedTiles.begin(), m_AnimatedTiles.end(),
                               [&](const AnimatedTile& tile) { return tile.object == obj; }),
                m_AnimatedTiles.end()
            );
        }

        m_Map[x][y] = Cell::Empty;
        m_TileObjects[x][y] = nullptr;
        m_QuestionBlockUsed[x][y] = false;
        m_HiddenQuestionBlocks[x][y] = false;
        m_QuestionBlockLoot[x][y] = LootType::Coin;
        m_QuestionBlockRemainingHits[x][y] = 1;
        return true;
    }

    bool CollectCoin(int x, int y) {
        if (GetCell(x, y) != Cell::Coin) return false;
        const float worldX = GetWorldLeft() + x * TILE_SIZE + TILE_SIZE / 2.0f;
        const float worldY = (m_Height * TILE_SIZE) / 2.0f - y * TILE_SIZE - TILE_SIZE / 2.0f;
        m_CoinCollectEvents.push({ { worldX, worldY } });
        return ClearTile(x, y);
    }

    bool HitQuestionBlock(int x, int y, bool marioIsBig = false) {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return false;
        if (m_Map[x][y] != Cell::QuestionBlock || m_QuestionBlockUsed[x][y]) return false;

        float worldX = GetWorldLeft() + x * TILE_SIZE + TILE_SIZE / 2.0f;
        float worldY = (m_Height * TILE_SIZE) / 2.0f - y * TILE_SIZE;
        LootType spawnType = m_QuestionBlockLoot[x][y];
        if (spawnType == LootType::ProgressivePowerUp) {
            spawnType = marioIsBig ? LootType::FireFlower : LootType::RedMushroom;
        }
        m_SpawnEvents.push({ spawnType, { worldX, worldY } });

        m_QuestionBlockRemainingHits[x][y] = std::max(0, m_QuestionBlockRemainingHits[x][y] - 1);
        if (m_QuestionBlockRemainingHits[x][y] > 0) {
            return true;
        }

        m_QuestionBlockUsed[x][y] = true;
        m_HiddenQuestionBlocks[x][y] = false;

        const std::string usedPath = ResolveTilePath(
            Cell::QuestionBlock,
            m_IsUndergroundTheme ? "ug_question4.png" : "Question4.png"
        );
        if (m_TileObjects[x][y] != nullptr) {
            m_TileObjects[x][y]->SetVisible(true);
            m_TileObjects[x][y]->SetDrawable(std::make_shared<Util::Image>(usedPath));
        }

        m_AnimatedTiles.erase(
            std::remove_if(m_AnimatedTiles.begin(), m_AnimatedTiles.end(),
                           [&](const AnimatedTile& tile) { return tile.gridX == x && tile.gridY == y; }),
            m_AnimatedTiles.end()
        );

        return true;
    }

    bool BreakBrick(int x, int y) {
        if (GetCell(x, y) != Cell::Brick) return false;

        for (int offset = 1; offset <= 2; ++offset) {
            if (GetCell(x, y - offset) == Cell::Coin) {
                CollectCoin(x, y - offset);
            }
        }

        const float worldX = GetWorldLeft() + x * TILE_SIZE + TILE_SIZE / 2.0f;
        const float worldY = (m_Height * TILE_SIZE) / 2.0f - y * TILE_SIZE - TILE_SIZE / 2.0f;

        if (!ClearTile(x, y)) return false;

        m_BrickBreakEvents.push({ { worldX, worldY } });
        return true;
    }

    bool PollSpawnEvent(LootType& type, glm::vec2& position) {
        if (m_SpawnEvents.empty()) return false;
        const auto event = m_SpawnEvents.front();
        m_SpawnEvents.pop();
        type = event.type;
        position = event.position;
        return true;
    }

    bool PollBrickBreakEvent(glm::vec2& position) {
        if (m_BrickBreakEvents.empty()) return false;
        const auto event = m_BrickBreakEvents.front();
        m_BrickBreakEvents.pop();
        position = event.position;
        return true;
    }

    bool PollCoinCollectEvent(glm::vec2& position) {
        if (m_CoinCollectEvents.empty()) return false;
        const auto event = m_CoinCollectEvents.front();
        m_CoinCollectEvents.pop();
        position = event.position;
        return true;
    }

    void SetGoal(float x) {
        m_HasGoal = true;
        m_GoalX = x;
    }

    void ConfigureGoalVisuals(int poleGridX,
                              int flagGridX,
                              int flagGridY,
                              int poleBottomGridY,
                              int baseGridY,
                              const std::string& flagPath) {
        m_HasGoal = true;
        const float poleX = GetWorldLeft() + poleGridX * TILE_SIZE + TILE_SIZE * 0.5f;
        m_GoalX = poleX;
        m_GoalGroundY = (m_Height * TILE_SIZE) / 2.0f - baseGridY * TILE_SIZE;
        // Attach the flag to the pole instead of leaving a full tile gap.
        m_FlagX = poleX - TILE_SIZE * 0.5f;
        m_FlagTopY = (m_Height * TILE_SIZE) / 2.0f - flagGridY * TILE_SIZE - TILE_SIZE * 0.5f;
        m_FlagBottomY = (m_Height * TILE_SIZE) / 2.0f - poleBottomGridY * TILE_SIZE - TILE_SIZE * 0.5f;
        m_GoalSlideStartY = m_FlagTopY;

        m_GoalFlagObject = std::make_shared<Util::GameObject>();
        m_GoalFlagImage = std::make_shared<Util::Image>(ResolveBackgroundPath(flagPath));
        m_GoalFlagObject->SetDrawable(m_GoalFlagImage);
        m_GoalFlagObject->m_Transform.translation = { m_FlagX, m_FlagTopY };
        glm::vec2 flagSize = m_GoalFlagImage->GetSize();
        if (flagSize.x > 0.0f && flagSize.y > 0.0f) {
            const float uniformScale = TILE_SIZE / std::max(flagSize.x, flagSize.y);
            m_GoalFlagObject->m_Transform.scale = { uniformScale, uniformScale };
        } else {
            m_GoalFlagObject->m_Transform.scale = { 1.0f, 1.0f };
        }
        m_GoalFlagObject->SetZIndex(-9.0f);
    }

    bool HasGoal() const { return m_HasGoal; }
    float GetGoalX() const { return m_GoalX; }
    float GetGoalGroundY() const { return m_GoalGroundY; }
    float GetFlagTopY() const { return m_FlagTopY; }
    float GetFlagBottomY() const { return m_FlagBottomY; }
    float GetFlagX() const { return m_FlagX; }
    float GetGoalSlideStartY() const { return m_GoalSlideStartY; }
    void SetTransitionPipeEntryX(float x) {
        m_HasTransitionPipe = true;
        m_TransitionPipeEntryX = x;
    }
    bool HasTransitionPipe() const { return m_HasTransitionPipe; }
    float GetTransitionPipeEntryX() const { return m_TransitionPipeEntryX; }
    void SetFlagY(float y) {
        if (m_GoalFlagObject) {
            m_GoalFlagObject->m_Transform.translation.y = std::clamp(y, m_FlagBottomY, m_FlagTopY);
        }
    }

    void Update() {
        const float dt = std::max(0.001f, Util::Time::GetDeltaTimeMs() / 1000.0f);
        for (auto& tile : m_AnimatedTiles) {
            tile.animation.Update(dt);
            tile.image->SetImage(tile.animation.GetCurrentFramePath());
        }

        for (auto& platform : m_MovingPlatforms) {
            platform.previousCenter = platform.object->m_Transform.translation;
            platform.delta = { 0.0f, 0.0f };
            platform.wrappedThisFrame = false;

            float nextPosition = platform.motion == MovingPlatformMotion::Horizontal
                ? platform.object->m_Transform.translation.x
                : platform.object->m_Transform.translation.y;
            const float positiveLimit = platform.motion == MovingPlatformMotion::Horizontal
                ? platform.rightLimit
                : platform.topLimit;
            const float negativeLimit = platform.motion == MovingPlatformMotion::Horizontal
                ? platform.leftLimit
                : platform.bottomLimit;

            switch (platform.state) {
            case MovingPlatformState::MovingUp:
                nextPosition += platform.moveSpeed * dt;
                if (nextPosition >= positiveLimit) {
                    if (platform.cycle == MovingPlatformCycle::WrapUp) {
                        const float overshoot = nextPosition - positiveLimit;
                        nextPosition = negativeLimit + overshoot;
                        platform.wrappedThisFrame = true;
                    } else {
                        nextPosition = positiveLimit;
                    }
                    if (platform.cycle != MovingPlatformCycle::Bounce) {
                        platform.state = MovingPlatformState::MovingUp;
                    } else if (platform.waitTime > 0.0f) {
                        platform.state = MovingPlatformState::WaitingTop;
                        platform.waitTimer = platform.waitTime;
                    } else {
                        platform.state = MovingPlatformState::MovingDown;
                    }
                }
                break;
            case MovingPlatformState::MovingDown:
                nextPosition -= platform.moveSpeed * dt;
                if (nextPosition <= negativeLimit) {
                    if (platform.cycle == MovingPlatformCycle::WrapDown) {
                        const float overshoot = negativeLimit - nextPosition;
                        nextPosition = positiveLimit - overshoot;
                        platform.wrappedThisFrame = true;
                    } else {
                        nextPosition = negativeLimit;
                    }
                    if (platform.cycle != MovingPlatformCycle::Bounce) {
                        platform.state = MovingPlatformState::MovingDown;
                    } else if (platform.waitTime > 0.0f) {
                        platform.state = MovingPlatformState::WaitingBottom;
                        platform.waitTimer = platform.waitTime;
                    } else {
                        platform.state = MovingPlatformState::MovingUp;
                    }
                }
                break;
            case MovingPlatformState::WaitingTop:
                platform.waitTimer = std::max(0.0f, platform.waitTimer - dt);
                if (platform.waitTimer <= 0.0f) {
                    platform.state = MovingPlatformState::MovingDown;
                }
                break;
            case MovingPlatformState::WaitingBottom:
                platform.waitTimer = std::max(0.0f, platform.waitTimer - dt);
                if (platform.waitTimer <= 0.0f) {
                    platform.state = MovingPlatformState::MovingUp;
                }
                break;
            }

            if (platform.motion == MovingPlatformMotion::Horizontal) {
                platform.object->m_Transform.translation.x = nextPosition;
            } else {
                platform.object->m_Transform.translation.y = nextPosition;
            }
            platform.delta = platform.object->m_Transform.translation - platform.previousCenter;
        }

    }

    std::vector<CollisionBox> GetSolidCollisionBoxes() const {
        std::vector<CollisionBox> boxes;
        boxes.reserve(static_cast<std::size_t>(m_Width * m_Height));

        const float worldLeft = GetWorldLeft();
        const float worldTop = (m_Height * TILE_SIZE) / 2.0f;
        for (int x = 0; x < m_Width; ++x) {
            for (int y = 0; y < m_Height; ++y) {
                if (!IsSolidAt(x, y)) {
                    continue;
                }

                boxes.push_back({
                    {
                        worldLeft + x * TILE_SIZE + TILE_SIZE * 0.5f,
                        worldTop - y * TILE_SIZE - TILE_SIZE * 0.5f
                    },
                    { TILE_SIZE * 0.5f, TILE_SIZE * 0.5f }
                });
            }
        }

        return boxes;
    }

    std::vector<CollisionBox> GetMovingPlatformCollisionBoxes() const {
        std::vector<CollisionBox> boxes;
        boxes.reserve(m_MovingPlatforms.size());
        for (const auto& platform : m_MovingPlatforms) {
            boxes.push_back({ platform.object->m_Transform.translation, platform.halfExtents });
        }
        return boxes;
    }

    void DrawBackground(float viewX, float viewY = 0.0f) {
        for (auto& obj : m_BackgroundObjects) {
            auto oldPos = obj->m_Transform.translation;
            obj->m_Transform.translation.x -= viewX;
            obj->m_Transform.translation.y -= viewY;
            obj->Draw();
            obj->m_Transform.translation = oldPos;
        }

        if (m_GoalFlagObject != nullptr) {
            auto oldPos = m_GoalFlagObject->m_Transform.translation;
            m_GoalFlagObject->m_Transform.translation.x -= viewX;
            m_GoalFlagObject->m_Transform.translation.y -= viewY;
            m_GoalFlagObject->Draw();
            m_GoalFlagObject->m_Transform.translation = oldPos;
        }
    }

    void DrawTiles(float viewX, float viewY = 0.0f) {
        for (auto& obj : m_Objects) {
            auto oldPos = obj->m_Transform.translation;
            obj->m_Transform.translation.x -= viewX;
            obj->m_Transform.translation.y -= viewY;
            obj->Draw();
            obj->m_Transform.translation = oldPos;
        }
    }

    void Draw(float viewX, float viewY = 0.0f) {
        DrawBackground(viewX, viewY);
        DrawTiles(viewX, viewY);
    }

    void DrawForeground(float viewX, float viewY = 0.0f) {
        for (auto& obj : m_ForegroundObjects) {
            auto oldPos = obj->m_Transform.translation;
            obj->m_Transform.translation.x -= viewX;
            obj->m_Transform.translation.y -= viewY;
            obj->Draw();
            obj->m_Transform.translation = oldPos;
        }
    }

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    float GetTileSize() const { return TILE_SIZE; }

    float GetWorldLeft() const {
        return -(m_Width * TILE_SIZE) / 2.0f;
    }

    float GetWorldRight() const {
        return (m_Width * TILE_SIZE) / 2.0f;
    }
};

#endif
