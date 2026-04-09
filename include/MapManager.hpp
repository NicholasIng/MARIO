#ifndef MAP_MANAGER_HPP
#define MAP_MANAGER_HPP

#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <glm/vec2.hpp> // for glm::vec2

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include "Util/Logger.hpp"
#include "config.hpp"

enum class Cell { Empty, Wall, Brick, QuestionBlock, Pipe, Coin };

class MapManager {
private:
    std::vector<std::vector<Cell>> m_Map;
    std::vector<std::vector<std::shared_ptr<Util::GameObject>>> m_TileObjects;
    std::vector<std::vector<std::shared_ptr<Util::GameObject>>> m_BackgroundTileObjects;

    // solid / gameplay objects
    std::vector<std::shared_ptr<Util::GameObject>> m_Objects;

    // decorative background objects
    std::vector<std::shared_ptr<Util::GameObject>> m_BackgroundObjects;

    int m_Width = 0;
    int m_Height = 0;

    const float TILE_SIZE = 48.0f;

public:
    MapManager() = default;

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

        for (const auto& subdir : { fs::path("Tiles"), fs::path("Background"), fs::path("character") }) {
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
        m_Objects.clear();
        m_BackgroundObjects.clear();
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

        obj->m_Transform.translation = { xPos, yPos };

        glm::vec2 imgSize = img->GetSize();
        float scale = 1.0f;
        if (imgSize.x > 0.0f && imgSize.y > 0.0f) {
            obj->m_Transform.scale = {
                (tileSpanX * TILE_SIZE) / imgSize.x,
                (tileSpanY * TILE_SIZE) / imgSize.y
            };
        } else {
            obj->m_Transform.scale = { scale, scale };
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

    void Draw(float viewX) {
        for (auto& obj : m_BackgroundObjects) {
            auto oldPos = obj->m_Transform.translation;
            obj->m_Transform.translation.x -= viewX;
            obj->Draw();
            obj->m_Transform.translation = oldPos;
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
