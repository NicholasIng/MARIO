#ifndef MAP_MANAGER_HPP
#define MAP_MANAGER_HPP

#include <vector>
#include <memory>
#include <string>
#include "Tile.hpp"

class MapManager {
public:
    MapManager() = default;

    // Load map from a 2D vector of integers/chars
    void LoadMap(const std::vector<std::vector<char>>& mapData) {
        float tileSize = 16.0f * 3.0f; // 16px base * 3x scale = 48px
        
        for (size_t row = 0; row < mapData.size(); ++row) {
            for (size_t col = 0; col < mapData[row].size(); ++col) {
                float x = col * tileSize - 300.0f; // Offset to center/align
                float y = -(row * tileSize) + 200.0f; // Invert Y for PTSD coords

                char cell = mapData[row][col];
                if (cell == 'G') {
                    m_Tiles.push_back(std::make_shared<Tile>(RESOURCE_DIR "/Image/Tiles/Ground.png", Tile::Type::GROUND, x, y));
                } else if (cell == '?') {
                    m_Tiles.push_back(std::make_shared<Tile>(RESOURCE_DIR "/Image/Tiles/Question.png", Tile::Type::QUESTION, x, y));
                }
            }
        }
    }

    void Draw() {
        for (auto& tile : m_Tiles) {
            tile->Draw();
        }
    }

    const std::vector<std::shared_ptr<Tile>>& GetTiles() const { return m_Tiles; }

private:
    std::vector<std::shared_ptr<Tile>> m_Tiles; // Managed memory
};

#endif