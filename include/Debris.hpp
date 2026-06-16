#pragma once

#include "GameImage.hpp"
#include "Util/GameObject.hpp"
#include <memory>
#include <string>

class Debris : public Util::GameObject {
public:
    enum class Piece {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    Debris(float x, float y, Piece piece);

    void Update();

    bool IsExpired() const { return m_Expired; }

private:
    std::shared_ptr<GameImage> m_Image;
    glm::vec2 m_Velocity{0.0f, 0.0f};
    float m_LifeTimer = 0.0f;
    bool m_Expired = false;
};
