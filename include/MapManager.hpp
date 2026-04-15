#ifndef MAP_MANAGER_HPP
#define MAP_MANAGER_HPP

#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <queue>
#include <random>
#include <glm/vec2.hpp> // for glm::vec2

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"
#include "Animation.hpp"
#include "config.hpp"

enum class Cell { Empty, Wall, Brick, QuestionBlock, Pipe, Coin };
enum class LootType { RedMushroom, FireFlower };

class MapManager {
private:
    std::vector<std::vector<Cell>> m_Map;
    std::vector<std::vector<std::shared_ptr<Util::GameObject>>> m_TileObjects;
    std::vector<std::vector<std::shared_ptr<Util::GameObject>>> m_BackgroundTileObjects;

    // solid / gameplay objects
    std::vector<std::shared_ptr<Util::GameObject>> m_Objects;

    // decorative background objects
    std::vector<std::shared_ptr<Util::GameObject>> m_BackgroundObjects;

    struct AnimatedTile {
        std::shared_ptr<Util::GameObject> object;
        std::shared_ptr<Util::Image> image;
        Animation animation;
        int gridX = 0;
        int gridY = 0;
    };
    std::vector<AnimatedTile> m_AnimatedTiles;
    std::vector<std::vector<bool>> m_QuestionBlockUsed;
    struct SpawnEvent {
        LootType type;
        glm::vec2 position;
    };
    std::queue<SpawnEvent> m_SpawnEvents;
    struct BrickBreakEvent {
        glm::vec2 position;
    };
    std::queue<BrickBreakEvent> m_BrickBreakEvents;
    bool m_HasGoal = false;
    float m_GoalX = 0.0f;
    float m_GoalGroundY = 0.0f;
    float m_FlagTopY = 0.0f;
    float m_FlagBottomY = 0.0f;
    float m_FlagX = 0.0f;
    std::shared_ptr<Util::GameObject> m_GoalFlagObject;
    std::shared_ptr<Util::Image> m_GoalFlagImage;
    mutable std::mt19937 m_Rng{std::random_device{}()};

    int m_Width = 0;
    int m_Height = 0;

    const float TILE_SIZE = 48.0f;

public:
    MapManager() = default;

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
                LOG_WARN("Requested tile '{}' not found; using '{}'", requested, found);
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
                fallback = FindImageByFilename(imageRoot, "Coin.png");
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
                LOG_WARN("Requested background '{}' not found; using '{}'", requested, found);
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

        m_Map.assign(width, std::vector<Cell>(height, Cell::Empty));
        m_TileObjects.assign(width, std::vector<std::shared_ptr<Util::GameObject>>(height, nullptr));
        m_BackgroundTileObjects.assign(width, std::vector<std::shared_ptr<Util::GameObject>>(height, nullptr));
        m_QuestionBlockUsed.assign(width, std::vector<bool>(height, false));
        m_Objects.clear();
        m_BackgroundObjects.clear();
        m_AnimatedTiles.clear();
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
        m_GoalFlagObject = nullptr;
        m_GoalFlagImage = nullptr;
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
            scale.x = (tileSpanX * TILE_SIZE) / imgSize.x;
            scale.y = (tileSpanY * TILE_SIZE) / imgSize.y;
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
                    ResolveTilePath(Cell::QuestionBlock, "Question1.png"),
                    ResolveTilePath(Cell::QuestionBlock, "Question2.png"),
                    ResolveTilePath(Cell::QuestionBlock, "Question3.png")
                };
            } else {
                frames = {
                    ResolveTilePath(Cell::Coin, "coin1.png"),
                    ResolveTilePath(Cell::Coin, "coin2.png"),
                    ResolveTilePath(Cell::Coin, "coin3.png"),
                    ResolveTilePath(Cell::Coin, "coin4.png")
                };
                frameDuration = 0.09f;
            }
            m_AnimatedTiles.push_back({ obj, img, Animation(frames, frameDuration), gridX, gridY });
        }
    }

    void AddTile(int gridX, int gridY, Cell type, const std::string& texturePath) {
        AddTileSprite(gridX, gridY, 1, 1, type, texturePath);
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

    Cell GetCell(int x, int y) const {
        if (x >= 0 && x < m_Width && y >= 0 && y < m_Height) {
            return m_Map[x][y];
        }
        // return Empty for out-of-range requests (safer for callers)
        return Cell::Empty;
    }

    bool IsSolidAt(int x, int y) const {
        return IsSolidCell(GetCell(x, y));
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
        return true;
    }

    bool CollectCoin(int x, int y) {
        if (GetCell(x, y) != Cell::Coin) return false;
        return ClearTile(x, y);
    }

    bool HitQuestionBlock(int x, int y) {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return false;
        if (m_Map[x][y] != Cell::QuestionBlock || m_QuestionBlockUsed[x][y]) return false;

        m_QuestionBlockUsed[x][y] = true;

        const std::string usedPath = ResolveTilePath(Cell::QuestionBlock, "Question4.png");
        if (m_TileObjects[x][y] != nullptr) {
            m_TileObjects[x][y]->SetDrawable(std::make_shared<Util::Image>(usedPath));
        }

        m_AnimatedTiles.erase(
            std::remove_if(m_AnimatedTiles.begin(), m_AnimatedTiles.end(),
                           [&](const AnimatedTile& tile) { return tile.gridX == x && tile.gridY == y; }),
            m_AnimatedTiles.end()
        );

        std::uniform_int_distribution<int> dist(0, 1);
        LootType type = dist(m_Rng) == 0 ? LootType::RedMushroom : LootType::FireFlower;
        float worldX = GetWorldLeft() + x * TILE_SIZE + TILE_SIZE / 2.0f;
        float worldY = (m_Height * TILE_SIZE) / 2.0f - y * TILE_SIZE;
        m_SpawnEvents.push({ type, { worldX, worldY } });
        return true;
    }

    bool BreakBrick(int x, int y) {
        if (GetCell(x, y) != Cell::Brick) return false;

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

    void SetGoal(float x) {
        m_HasGoal = true;
        m_GoalX = x;
    }

    void ConfigureGoalVisuals(int poleGridX,
                              int flagGridX,
                              int flagGridY,
                              int poleBottomGridY,
                              const std::string& flagPath) {
        m_HasGoal = true;
        m_GoalX = GetWorldLeft() + poleGridX * TILE_SIZE + TILE_SIZE * 0.5f;
        m_GoalGroundY = (m_Height * TILE_SIZE) / 2.0f - poleBottomGridY * TILE_SIZE;
        m_FlagX = GetWorldLeft() + flagGridX * TILE_SIZE + TILE_SIZE * 0.5f;
        m_FlagTopY = (m_Height * TILE_SIZE) / 2.0f - flagGridY * TILE_SIZE - TILE_SIZE * 0.5f;
        m_FlagBottomY = (m_Height * TILE_SIZE) / 2.0f - poleBottomGridY * TILE_SIZE - TILE_SIZE * 0.5f;

        m_GoalFlagObject = std::make_shared<Util::GameObject>();
        m_GoalFlagImage = std::make_shared<Util::Image>(ResolveBackgroundPath(flagPath));
        m_GoalFlagObject->SetDrawable(m_GoalFlagImage);
        m_GoalFlagObject->m_Transform.translation = { m_FlagX, m_FlagTopY };
        m_GoalFlagObject->m_Transform.scale = { 3.0f, 3.0f };
        m_GoalFlagObject->SetZIndex(-9.0f);
    }

    bool HasGoal() const { return m_HasGoal; }
    float GetGoalX() const { return m_GoalX; }
    float GetGoalGroundY() const { return m_GoalGroundY; }
    float GetFlagTopY() const { return m_FlagTopY; }
    float GetFlagBottomY() const { return m_FlagBottomY; }
    float GetFlagX() const { return m_FlagX; }
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

    }

    void Draw(float viewX) {
        for (auto& obj : m_BackgroundObjects) {
            auto oldPos = obj->m_Transform.translation;
            obj->m_Transform.translation.x -= viewX;
            obj->Draw();
            obj->m_Transform.translation = oldPos;
        }

        if (m_GoalFlagObject != nullptr) {
            auto oldPos = m_GoalFlagObject->m_Transform.translation;
            m_GoalFlagObject->m_Transform.translation.x -= viewX;
            m_GoalFlagObject->Draw();
            m_GoalFlagObject->m_Transform.translation = oldPos;
        }

        for (auto& obj : m_Objects) {
            auto oldPos = obj->m_Transform.translation;
            obj->m_Transform.translation.x -= viewX;
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
