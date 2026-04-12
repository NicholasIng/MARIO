#pragma once

#include "Animation.hpp"
#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <memory>

class Fireball : public Util::GameObject {
public:
    Fireball(float x, float y, float direction);

    void Update();
    void Explode();

    bool IsExpired() const { return m_Expired; }
    bool IsExploding() const { return m_Exploding; }
    glm::vec2 GetHalfExtents() const { return { 12.0f, 12.0f }; }

private:
    std::shared_ptr<Util::Image> m_Image;
    std::unique_ptr<Animation> m_FlyingAnimation;
    std::unique_ptr<Animation> m_HitAnimation;
    float m_Direction = 1.0f;
    float m_VelocityY = 0.0f;
    float m_HitTimer = 0.0f;
    bool m_Exploding = false;
    bool m_Expired = false;
};
