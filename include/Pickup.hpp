#pragma once

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include "Animation.hpp"
#include "MapManager.hpp"
#include <memory>
#include <string>

class Pickup : public Util::GameObject {
public:
    Pickup(LootType type, float x, float y);
    void Update();
    bool IsCollected() const { return m_Collected; }
    void Collect() { m_Collected = true; SetVisible(false); }
    bool ConsumeAutoAward();
    glm::vec2 GetHalfExtents() const { return { 20.0f, 24.0f }; }
    LootType GetType() const { return m_Type; }

private:
    void UpdateRise(float dt);
    void UpdateCoinPop(float dt);

    static constexpr float BASE_SCALE = 3.0f;
    static constexpr float RISE_DISTANCE = 48.0f;
    static constexpr float RISE_DURATION = 0.3f;
    static constexpr float COIN_POP_DURATION = 0.5f;
    static constexpr float COIN_POP_HEIGHT = 88.0f;
    static constexpr float SPAWN_SLIDE_DISTANCE = 20.0f;

    std::shared_ptr<Util::Image> m_Image;
    std::unique_ptr<Animation> m_Animation;
    LootType m_Type;
    bool m_Collected = false;
    bool m_AutoAwardPending = false;
    float m_RiseElapsed = 0.0f;
    float m_CoinPopElapsed = 0.0f;
    float m_SpawnStartX = 0.0f;
    float m_SpawnStartY = 0.0f;
    bool m_HasLanded = false;
    float m_HorizontalDirection = 1.0f;
    float m_VelocityY = 0.0f;
};
