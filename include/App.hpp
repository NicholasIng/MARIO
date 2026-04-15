#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp"
#include "Mario.hpp"
#include "MapManager.hpp"
#include "Enemy.hpp"
#include "Fireball.hpp"
#include "Pickup.hpp"
#include "Debris.hpp"
#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <memory>
#include <vector>

class App {
public:
    enum class State { START, UPDATE, END };
    State GetCurrentState() const { return m_CurrentState; }

    void Start();
    void Update();
    void End();

private:
    enum class GoalSequenceStage { None, Sliding, Walking, Entering, Finished };

    State m_CurrentState = State::START;

    std::unique_ptr<Mario> m_Mario;
    std::vector<std::unique_ptr<Enemy>> m_Enemies;
    std::vector<std::unique_ptr<Fireball>> m_Fireballs;
    std::vector<std::unique_ptr<Pickup>> m_Pickups;
    std::vector<std::unique_ptr<Debris>> m_Debris;
    std::shared_ptr<Util::GameObject> m_CastleObject;
    std::shared_ptr<Util::Image> m_CastleImage;

    // camera/view like the youtuber's view_x
    float m_ViewX = 0.0f;
    float m_FireballCooldown = 0.0f;
    GoalSequenceStage m_GoalSequenceStage = GoalSequenceStage::None;
    float m_GoalSequenceTimer = 0.0f;
    float m_CastleDoorX = 0.0f;
    float m_GoalFlagMarioOffsetY = 0.0f;

    void StartGoalSequence();
    void UpdateGoalSequence(float dt);
};

#endif
