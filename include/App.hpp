#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp"
#include "Mario.hpp"
#include "MapManager.hpp"
#include <memory>

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

    // camera/view like the youtuber's view_x
    float m_ViewX = 0.0f;
};

#endif