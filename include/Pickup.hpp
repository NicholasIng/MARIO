#pragma once

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include "MapManager.hpp"
#include <memory>
#include <string>

class Pickup : public Util::GameObject {
public:
    Pickup(LootType type, float x, float y);
    void Update();
    bool IsCollected() const { return m_Collected; }
    void Collect() { m_Collected = true; SetVisible(false); }
    glm::vec2 GetHalfExtents() const { return { 18.0f, 18.0f }; }
    LootType GetType() const { return m_Type; }

private:
    std::shared_ptr<Util::Image> m_Image;
    LootType m_Type;
    bool m_Collected = false;
    float m_RiseRemaining = 24.0f;
};
