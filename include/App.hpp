#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp"
#include "Mario.hpp"
#include "MapManager.hpp"
#include "Enemy.hpp"    
#include "Fireball.hpp"
#include "Pickup.hpp"
#include "Debris.hpp"
#include "Util/BGM.hpp"
#include "Util/Color.hpp"
#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include "Util/SFX.hpp"
#include <memory>
#include <string>
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
    enum class ScreenState { Title, LevelIntro, Gameplay, Paused, StatusMessage, TransitionScene };
    enum class StatusMessageAction { None, ReloadLevel, ShowGameOver, RestartToTitle };
    enum class MusicTrack { None, GroundTheme, InvincibilityTheme };
    enum class LevelIntroPurpose { StartLevel, PostGoalTransition };
    struct PendingEnemySpawn {
        glm::vec2 position;
        EnemyKind kind = EnemyKind::Goomba;
        bool activated = false;
    };
    struct SpriteText {
        std::vector<std::shared_ptr<Util::GameObject>> glyphs;
        std::string content;
        glm::vec2 position = { 0.0f, 0.0f };
        glm::vec2 scale = { 1.0f, 1.0f };
        float spacing = 0.0f;
        float lineHeight = 0.0f;
        float zIndex = 50.0f;
        bool layoutDirty = true;
    };
    struct AnimatedSprite {
        std::vector<std::shared_ptr<Util::Image>> frames;
        std::shared_ptr<Util::GameObject> object;
        float frameDuration = 0.1f;
        float timer = 0.0f;
        std::size_t frameIndex = 0;
    };
    struct FloatingText {
        SpriteText text;
        std::string value;
        glm::vec2 worldPosition = { 0.0f, 0.0f };
        float lifetime = 0.0f;
        float riseSpeed = 0.0f;
    };
    struct AudioBank {
        std::unique_ptr<Util::BGM> groundTheme;
        std::unique_ptr<Util::BGM> invincibilityTheme;
        std::unique_ptr<Util::SFX> oneUp;
        std::unique_ptr<Util::SFX> bowserFalls;
        std::unique_ptr<Util::SFX> bowserFire;
        std::unique_ptr<Util::SFX> breakBlock;
        std::unique_ptr<Util::SFX> bump;
        std::unique_ptr<Util::SFX> coin;
        std::unique_ptr<Util::SFX> fireball;
        std::unique_ptr<Util::SFX> fireworks;
        std::unique_ptr<Util::SFX> flagpole;
        std::unique_ptr<Util::SFX> gameOver;
        std::unique_ptr<Util::SFX> jumpSmall;
        std::unique_ptr<Util::SFX> jumpSuper;
        std::unique_ptr<Util::SFX> kick;
        std::unique_ptr<Util::SFX> marioDie;
        std::unique_ptr<Util::SFX> pause;
        std::unique_ptr<Util::SFX> pipe;
        std::unique_ptr<Util::SFX> powerUp;
        std::unique_ptr<Util::SFX> powerUpAppears;
        std::unique_ptr<Util::SFX> stageClear;
        std::unique_ptr<Util::SFX> stomp;
        std::unique_ptr<Util::SFX> vine;
        std::unique_ptr<Util::SFX> warning;
        std::unique_ptr<Util::SFX> worldClear;
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
    int m_DisplayedLevelTime = 400;
    float m_LevelIntroTimer = 0.0f;
    float m_StatusMessageTimer = 0.0f;
    float m_TitleBlinkTimer = 0.0f;
    float m_TransitionExitTimer = 0.0f;
    float m_TransitionPipeEntryX = 0.0f;
    float m_TransitionPipeEntryY = 0.0f;
    float m_TransitionPipeSinkDistance = 0.0f;
    MusicTrack m_ActiveMusic = MusicTrack::None;
    bool m_WasMarioDead = false;
    bool m_DeathWasTimeout = false;
    bool m_LowTimeWarningPlayed = false;
    bool m_GoalCelebrationPlayed = false;
    bool m_TransitionPipeReached = false;
    bool m_TransitionPipeSoundPlayed = false;
    bool m_TransitionMarioHidden = false;
    Util::Color m_SkyColor = Util::Color(90, 147, 235, 255);
    int m_TitleSelection = 0;
    LevelIntroPurpose m_LevelIntroPurpose = LevelIntroPurpose::StartLevel;
    AudioBank m_Audio;
    AnimatedSprite m_HudCoinIcon;
    AnimatedSprite m_TitleCoinIcon;
    std::shared_ptr<Util::GameObject> m_TitleLogoImage;
    std::shared_ptr<Util::GameObject> m_TitleNintendoText;
    std::shared_ptr<Util::GameObject> m_TitleCloudLeft;
    std::shared_ptr<Util::GameObject> m_TitleCloudRight;
    std::shared_ptr<Util::GameObject> m_TitleMountain;
    std::shared_ptr<Util::GameObject> m_TitleBushLeft;
    std::shared_ptr<Util::GameObject> m_TitleBush;
    std::shared_ptr<Util::GameObject> m_TitleMario;
    std::shared_ptr<Util::GameObject> m_IntroMario;
    std::shared_ptr<Util::GameObject> m_TitleCursor;
    std::vector<std::shared_ptr<Util::GameObject>> m_TitleGroundTiles;
    SpriteText m_HudMarioLabel;
    SpriteText m_HudScoreValue;
    SpriteText m_HudWorldLabel;
    SpriteText m_HudWorldValue;
    SpriteText m_HudTimeLabel;
    SpriteText m_HudTimeValue;
    SpriteText m_HudCoinValue;
    SpriteText m_TitleOption1;
    SpriteText m_TitleOption2;
    SpriteText m_TitleTopScore;
    SpriteText m_IntroWorldText;
    SpriteText m_IntroLivesText;
    SpriteText m_StatusMessageText;
    StatusMessageAction m_StatusMessageAction = StatusMessageAction::None;
    std::vector<FloatingText> m_FloatingTexts;

    void StartGoalSequence();
    void UpdateGoalSequence(float dt);
    void ActivateNearbyEnemies();
    void UpdateTitleScreen(float dt);
    void UpdateLevelIntro(float dt);
    void UpdatePaused(float dt);
    void UpdateStatusMessage(float dt);
    void UpdateGameplay(float dt);
    void UpdateTransitionScene(float dt);
    void RenderTitleScreen();
    void RenderLevelIntro();
    void RenderPaused();
    void RenderStatusMessage();
    void RenderGameplay();
    void InitializeUi();
    void InitializeAudio();
    void InitializeAnimatedSprite(AnimatedSprite& sprite,
                                  const std::vector<std::string>& framePaths,
                                  const glm::vec2& position,
                                  const glm::vec2& scale,
                                  float zIndex,
                                  float frameDuration);
    void AdvanceAnimatedSprite(AnimatedSprite& sprite, float dt);
    void ConfigureSpriteText(SpriteText& spriteText,
                             const glm::vec2& position,
                             const glm::vec2& scale,
                             float spacing,
                             float lineHeight,
                             float zIndex);
    void LayoutSpriteText(SpriteText& spriteText);
    void SetSpriteText(SpriteText& spriteText, const std::string& text);
    void DrawSpriteText(const SpriteText& spriteText) const;
    std::string GetFontSpritePath(char character) const;
    void DrawHud();
    void DrawUiObject(const std::shared_ptr<Util::GameObject>& object) const;
    void RefreshHudText();
    void ResetGameSession();
    void LoadLevel();
    bool LoadSceneSketch(const std::string& sketchPath, bool preserveProgress);
    std::string ResolveTransitionSketchPath() const;
    void BeginPostGoalTransition();
    void LoadTransitionScene();
    void RenderSceneWorld(bool drawCastle);
    void HandleMarioDeath();
    void TogglePause();
    void PlayTitleMusic();
    void PlayGameplayMusic(bool restart = false);
    void PlayInvincibilityMusic(bool restart = false);
    void UpdateGameplayMusic();
    void StopMusic(int fadeMs = 0);
    void PauseMusic();
    void ResumeMusic();
    void PlaySfx(Util::SFX* sfx, int loop = 0, int duration = -1);
    void BeginStatusMessage(const std::string& message, float duration, StatusMessageAction action);
    void AddScore(int points);
    void AwardPoints(int points, const glm::vec2& worldPosition, const std::string& popupText = "");
    void SpawnFloatingText(const std::string& text, const glm::vec2& worldPosition);
    void UpdateFloatingTexts(float dt);
    void DrawFloatingTexts();
    void StartLevelIntro(float duration);
};

#endif
