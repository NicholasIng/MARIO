#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp"
#include "Mario.hpp"
#include "MapManager.hpp"
#include "Enemy.hpp"
#include "Fireball.hpp"
#include "Pickup.hpp"
#include "Debris.hpp"
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
    State m_CurrentState = State::START;

    std::unique_ptr<Mario> m_Mario;
    std::vector<std::unique_ptr<Enemy>> m_Enemies;
    std::vector<std::unique_ptr<Fireball>> m_Fireballs;
    std::vector<std::unique_ptr<Pickup>> m_Pickups;
    std::vector<std::unique_ptr<Debris>> m_Debris;

    // camera/view like the youtuber's view_x
    float m_ViewX = 0.0f;
    float m_FireballCooldown = 0.0f;
};

#endif
