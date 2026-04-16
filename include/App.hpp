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
#include "Util/Text.hpp"
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
    enum class GoalSequenceStage { None, Sliding, PlayerControl, Entering, Finished };
    enum class ScreenState { Title, LevelIntro, Gameplay };
    struct PendingEnemySpawn {
        glm::vec2 position;
        bool activated = false;
    };
    struct HudText {
        std::shared_ptr<Util::Text> drawable;
        std::shared_ptr<Util::GameObject> object;
    };

    State m_CurrentState = State::START;
    ScreenState m_ScreenState = ScreenState::Title;

    std::unique_ptr<Mario> m_Mario;
    std::vector<std::unique_ptr<Enemy>> m_Enemies;
    std::vector<PendingEnemySpawn> m_PendingEnemySpawns;
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
    int m_Score = 0;
    int m_TopScore = 0;
    int m_Coins = 0;
    int m_Lives = 3;
    int m_World = 1;
    int m_Level = 1;
    float m_LevelTimer = 400.0f;
    float m_LevelIntroTimer = 0.0f;
    bool m_WasMarioDead = false;
    std::string m_FontPath;
    std::string m_PixelFontPath;
    Util::Color m_SkyColor = Util::Color(90, 147, 235, 255);
    std::shared_ptr<Util::GameObject> m_HudCoinIcon;
    std::shared_ptr<Util::GameObject> m_TitleMountain;
    std::shared_ptr<Util::GameObject> m_TitleBush;
    std::shared_ptr<Util::GameObject> m_TitleMario;
    std::shared_ptr<Util::GameObject> m_IntroMario;
    HudText m_HudMarioLabel;
    HudText m_HudScoreValue;
    HudText m_HudWorldLabel;
    HudText m_HudWorldValue;
    HudText m_HudTimeLabel;
    HudText m_HudTimeValue;
    HudText m_HudCoinValue;
    HudText m_TitleLogo;
    HudText m_TitleCopyright;
    HudText m_TitleOption1;
    HudText m_TitleOption2;
    HudText m_TitleTopScore;
    HudText m_TitlePressStart;
    HudText m_IntroWorldText;
    HudText m_IntroLivesText;

    void StartGoalSequence();
    void UpdateGoalSequence(float dt);
    void ActivateNearbyEnemies();
    void UpdateTitleScreen(float dt);
    void UpdateLevelIntro(float dt);
    void UpdateGameplay(float dt);
    void RenderTitleScreen();
    void RenderLevelIntro();
    void RenderGameplay();
    void InitializeUi();
    HudText CreateTextObject(const std::string& text,
                             int size,
                             const glm::vec2& position,
                             const Util::Color& color,
                             const glm::vec2& scale = { 1.0f, 1.0f });
    void DrawHud();
    void DrawUiObject(const std::shared_ptr<Util::GameObject>& object) const;
    void RefreshHudText();
    void AddScore(int points);
    void StartLevelIntro(float duration);
};

#endif
