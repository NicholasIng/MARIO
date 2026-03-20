#ifndef ANIMATION_HPP
#define ANIMATION_HPP

#include <vector>
#include <string>

class Animation {
public:
    Animation(const std::vector<std::string>& paths, float frameInterval)
        : m_Paths(paths), m_FrameInterval(frameInterval), m_Timer(0.0f), m_CurrentFrame(0) {
    }

    std::string GetCurrentFramePath() const {
        return m_Paths[m_CurrentFrame];
    }

    void Update(float dt) {
        if (m_Paths.size() <= 1) return; // Don't tick for single-frame animations

        m_Timer += dt;
        if (m_Timer >= m_FrameInterval) {
            m_Timer = 0.0f;
            m_CurrentFrame = (m_CurrentFrame + 1) % m_Paths.size();
        }
    }

    void Reset() {
        m_CurrentFrame = 0;
        m_Timer = 0.0f;
    }

private:
    std::vector<std::string> m_Paths;
    float m_FrameInterval;
    float m_Timer;
    int m_CurrentFrame;
};

#endif