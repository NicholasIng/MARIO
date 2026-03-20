#ifndef TILE_HPP
#define TILE_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <memory>

class Tile : public Util::GameObject {
public:
    enum class Type { GROUND, BRICK, QUESTION, PIPE };

    Tile(const std::string& imagePath, Type type, float x, float y) : m_Type(type) {
        SetDrawable(std::make_unique<Util::Image>(imagePath));
        m_Transform.translation = { x, y };
        m_Transform.scale = { 3.0f, 3.0f }; // Match Mario's 3x scale
        m_ZIndex = 5.0f; // Below Mario (who is at 10.0f)
    }

    Type GetType() const { return m_Type; }

private:
    Type m_Type;
};

#endif