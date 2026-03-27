#ifndef MAP_MANAGER_HPP
#define MAP_MANAGER_HPP

#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <filesystem>
#include <glm/vec2.hpp> // for glm::vec2

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include "Util/Logger.hpp"
#include "config.hpp"

enum class Cell { Empty, Wall, Brick, QuestionBlock, Pipe, Coin };

class MapManager {
private:
    std::vector<std::vector<Cell>> m_Map;

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

    // helper: ensure path exists or try sensible fallbacks
    static std::string ResolveTilePath(Cell type, const std::string &requested) {
        namespace fs = std::filesystem;
        fs::path req = requested;
        if (fs::exists(req)) return requested;

        fs::path root = ResourceRoot();

        // try requested filename under resource root (handles absolute vs relative mismatches)
        if (!req.filename().empty()) {
            fs::path candidate = root / "image" / "Tiles" / req.filename();
            if (fs::exists(candidate)) {
                LOG_WARN("Requested tile '{}' not found; using '{}'", requested, candidate.string());
                return candidate.string();
            }
        }

        // fallback mapping by Cell type
        switch (type) {
            case Cell::Wall:
                if (fs::exists(root / "image" / "Tiles" / "Ground.png"))
                    return (root / "image" / "Tiles" / "Ground.png").string();
                break;
            case Cell::Brick:
                if (fs::exists(root / "image" / "Tiles" / "Brick.png"))
                    return (root / "image" / "Tiles" / "Brick.png").string();
                break;
            case Cell::QuestionBlock:
                if (fs::exists(root / "image" / "Tiles" / "Question.png"))
                    return (root / "image" / "Tiles" / "Question.png").string();
                break;
            case Cell::Pipe:
                if (fs::exists(root / "image" / "Tiles" / "Pipe.png"))
                    return (root / "image" / "Tiles" / "Pipe.png").string();
                break;
            case Cell::Coin:
                if (fs::exists(root / "image" / "Tiles" / "Coin.png"))
                    return (root / "image" / "Tiles" / "Coin.png").string();
                break;
            default:
                break;
        }

        // last resort: try any tile in Tiles folder
        if (fs::exists(root / "image" / "Tiles")) {
            for (auto &entry : fs::directory_iterator(root / "image" / "Tiles")) {
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

        // try requested filename under resource root
        if (!req.filename().empty()) {
            fs::path candidate = root / "image" / "Background" / req.filename();
            if (fs::exists(candidate)) {
                LOG_WARN("Requested background '{}' not found; using '{}'", requested, candidate.string());
                return candidate.string();
            }
        }

        // try keyword-based fallback
        std::string name = req.filename().string();
        if (name.find("Cloud") != std::string::npos || name.find("cloud") != std::string::npos) {
            if (fs::exists(root / "image" / "Background" / "Cloud.png"))
                return (root / "image" / "Background" / "Cloud.png").string();
        }
        if (name.find("Grass") != std::string::npos || name.find("grass") != std::string::npos) {
            if (fs::exists(root / "image" / "Background" / "Grass.png"))
                return (root / "image" / "Background" / "Grass.png").string();
        }
        if (name.find("Hill") != std::string::npos || name.find("hill") != std::string::npos) {
            if (fs::exists(root / "image" / "Background" / "HillFill.png"))
                return (root / "image" / "Background" / "HillFill.png").string();
        }
        if (name.find("Flag") != std::string::npos || name.find("flag") != std::string::npos) {
            if (fs::exists(root / "image" / "Background" / "Flagpole.png"))
                return (root / "image" / "Background" / "Flagpole.png").string();
        }

        // fallback: any background image
        if (fs::exists(root / "image" / "Background")) {
            for (auto &entry : fs::directory_iterator(root / "image" / "Background")) {
                if (!entry.is_directory()) {
                    LOG_WARN("Requested background '{}' not found; using '{}'", requested, entry.path().string());
                    return entry.path().string();
                }
            }
        }

        LOG_WARN("Requested background '{}' not found and no fallback available; using original path", requested);
        return requested;
    }

    void SetMapSize(int width, int height) {
        m_Width = width;
        m_Height = height;

        m_Map.assign(width, std::vector<Cell>(height, Cell::Empty));
        m_Objects.clear();
        m_BackgroundObjects.clear();
    }

    void AddTile(int gridX, int gridY, Cell type, const std::string& texturePath) {
        if (gridX < 0 || gridX >= m_Width || gridY < 0 || gridY >= m_Height)
            return;

        m_Map[gridX][gridY] = type;

        // resolve texture path (try fallbacks if missing)
        std::string resolvedPath = ResolveTilePath(type, texturePath);

        auto obj = std::make_shared<Util::GameObject>();

        // use shared_ptr so we can query the image size to compute scale
        auto img = std::make_shared<Util::Image>(resolvedPath);
        obj->SetDrawable(img);

        float xPos = -(m_Width * TILE_SIZE) / 2.0f + (gridX * TILE_SIZE) + TILE_SIZE / 2.0f;
        float yPos = (m_Height * TILE_SIZE) / 2.0f - (gridY * TILE_SIZE) - TILE_SIZE / 2.0f;

        obj->m_Transform.translation = { xPos, yPos };

        // compute scale such that image pixel width maps to TILE_SIZE world units
        glm::vec2 imgSize = img->GetSize();
        float scale = 1.0f;
        if (imgSize.x > 0.0f) {
            scale = TILE_SIZE / imgSize.x;
        }
        obj->m_Transform.scale = { scale, scale };

        obj->SetZIndex(1.0f);

        m_Objects.push_back(obj);
    }

    void AddBackgroundTile(int gridX, int gridY, const std::string& texturePath) {
        if (gridX < 0 || gridX >= m_Width || gridY < 0 || gridY >= m_Height)
            return;

        // resolve background path (try fallbacks if missing)
        std::string resolvedPath = ResolveBackgroundPath(texturePath);

        auto obj = std::make_shared<Util::GameObject>();

        auto img = std::make_shared<Util::Image>(resolvedPath);
        obj->SetDrawable(img);

        float xPos = -(m_Width * TILE_SIZE) / 2.0f + (gridX * TILE_SIZE) + TILE_SIZE / 2.0f;
        float yPos = (m_Height * TILE_SIZE) / 2.0f - (gridY * TILE_SIZE) - TILE_SIZE / 2.0f;

        obj->m_Transform.translation = { xPos, yPos };

        glm::vec2 imgSize = img->GetSize();
        float scale = 1.0f;
        if (imgSize.x > 0.0f) {
            scale = TILE_SIZE / imgSize.x;
        }
        obj->m_Transform.scale = { scale, scale };

        obj->SetZIndex(-10.0f);

        m_BackgroundObjects.push_back(obj);
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