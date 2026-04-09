#pragma once

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <string>

class Enemy : public Util::GameObject {
public:
    Enemy(float x, float y);
    void Update();

private:
    float m_StartX = 0.0f;
    float m_Direction = -1.0f;
    float m_Speed = 40.0f;
    std::string m_LeftPath;
    std::string m_RightPath;

    void RefreshSprite();
};

