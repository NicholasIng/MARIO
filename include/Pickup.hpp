#pragma once

#include "GameImage.hpp"
#include "Animation.hpp"
#include "MapManager.hpp"
#include "Util/GameObject.hpp"
#include <memory>
#include <string>

class Pickup : public Util::GameObject {
public:
    Pickup(LootType type, float x, float y, bool useQuestionCoinSprites = false, bool enableLaunchHop = false);
    void Update();
    bool IsCollected() const { return m_Collected; }
    void Collect() { m_Collected = true; SetVisible(false); }
    bool ConsumeAutoAward();
    glm::vec2 GetHalfExtents() const { return { 20.0f, 24.0f }; }
    LootType GetType() const { return m_Type; }

private:
    void UpdateRise(float dt);
    void UpdateCoinPop(float dt);
    void StartLaunchHop();

    static constexpr float BASE_SCALE = 3.0f;
    static constexpr float RISE_DISTANCE = 48.0f;
    static constexpr float RISE_DURATION = 0.3f;
    static constexpr float COIN_POP_DURATION = 0.5f;
    static constexpr float COIN_POP_HEIGHT = 88.0f;

    std::shared_ptr<GameImage> m_Image;
    std::unique_ptr<Animation> m_Animation;
    LootType m_Type;
    bool m_Collected = false;
    bool m_AutoAwardPending = false;
    bool m_UseQuestionCoinSprites = false;
    bool m_EnableLaunchHop = false;
    float m_RiseElapsed = 0.0f;
    float m_CoinPopElapsed = 0.0f;
    float m_SpawnStartX = 0.0f;
    float m_SpawnStartY = 0.0f;
    bool m_HasLanded = false;
    bool m_LaunchHopActive = false;
    bool m_LaunchHopStarted = false;
    float m_HorizontalDirection = 1.0f;
    float m_VelocityY = 0.0f;
    int m_IgnoredSpawnSupportGridY = -1;
};
