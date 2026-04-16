#pragma once

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <string>
#include <memory>

class Enemy : public Util::GameObject {
public:
    Enemy(float x, float y);
    void Update();
    void Stomp();
    void KillFlipped(float horizontalVelocity = 0.0f);
    bool IsAlive() const { return m_Alive; }
    bool IsDeadAndExpired() const { return !m_Alive && m_DeathFinished; }
    glm::vec2 GetHalfExtents() const { return { 20.0f, 24.0f }; }

private:
    std::shared_ptr<Util::Image> m_Image;
    float m_Direction = -1.0f;
    float m_Speed = 40.0f;
    float m_VelocityY = 0.0f;
    float m_VelocityX = 0.0f;
    float m_DeathTimer = 0.0f;
    float m_WalkAnimationTimer = 0.0f;
    bool m_Alive = true;
    bool m_FlippedDeath = false;
    bool m_DeathFinished = false;
    bool m_UseLeftWalkFrame = true;
    std::string m_LeftPath;
    std::string m_RightPath;
    std::string m_DeathPath;

    void RefreshSprite();
};

