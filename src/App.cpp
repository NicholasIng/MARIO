#include "App.hpp"
#include "Util/BGM.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "Util/SFX.hpp"
#include "Util/Time.hpp"
#include "MapManager.hpp"
#include "ConvertSketch.hpp"
#include "Enemy.hpp"
#include "AssetPaths.hpp"
#include "config.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

// global pointer for Mario collision
std::unique_ptr<MapManager> g_MapManager;

namespace {

constexpr float SKY_BLUE_R = 90.0f;
constexpr float SKY_BLUE_G = 147.0f;
constexpr float SKY_BLUE_B = 235.0f;
constexpr float CASTLE_TARGET_HEIGHT_TILES = 6.0f;
constexpr float CASTLE_OFFSET_TILES = 7.0f;
constexpr float CASTLE_END_INSET_TILES = 1.0f;
constexpr float GOAL_FINISH_DELAY = 1.2f;
constexpr float ENEMY_SPAWN_RANGE_X = WINDOW_WIDTH * 0.75f;
constexpr float LEVEL_INTRO_DURATION = 1.75f;
constexpr float FONT_GLYPH_SIZE = 8.0f;
constexpr int STARTING_TIMER = 400;
constexpr int STARTING_LIVES = 3;
constexpr float HUD_TEXT_SCALE = 3.0f;
constexpr float TITLE_TEXT_SCALE = 3.0f;
constexpr float INTRO_TEXT_SCALE = 4.0f;
constexpr float STATUS_MESSAGE_SCALE = 4.0f;
constexpr float COIN_FRAME_DURATION = 0.09f;
constexpr float TITLE_CURSOR_BLINK_DURATION = 0.18f;
constexpr float STATUS_MESSAGE_DURATION = 2.0f;
constexpr float TITLE_BG_R = 164.0f;
constexpr float TITLE_BG_G = 160.0f;
constexpr float TITLE_BG_B = 252.0f;
constexpr float UI_Z = 50.0f;
constexpr float TITLE_BG_Z = 24.0f;
constexpr float TITLE_DECOR_Z = 30.0f;
constexpr float TITLE_GROUND_Z = 20.0f;
constexpr float TITLE_LOGO_IMAGE_SCALE = 3.6f;
constexpr float TITLE_COPYRIGHT_SCALE = 2.9f;

std::string PadNumber(int value, int width) {
    std::ostringstream stream;
    stream << std::setw(width) << std::setfill('0') << std::max(0, value);
    return stream.str();
}

std::string WorldLabel(int world, int level) {
    return std::to_string(world) + "-" + std::to_string(level);
}

int DisplayLevelTime(float secondsRemaining) {
    return static_cast<int>(std::floor(std::max(0.0f, secondsRemaining)));
}

float CenteredTextX(const std::string& text, float scale, float spacing) {
    if (text.empty()) {
        return 0.0f;
    }

    const float advanceX = FONT_GLYPH_SIZE * scale + spacing;
    const float totalWidth = FONT_GLYPH_SIZE * scale + advanceX * static_cast<float>(text.size() - 1);
    return -totalWidth * 0.5f;
}

float GetLeftEdgeViewX(const MapManager* map) {
    if (map == nullptr) {
        return 0.0f;
    }

    const float halfScreen = WINDOW_WIDTH / 2.0f;
    const float minViewX = map->GetWorldLeft() + halfScreen;
    const float maxViewX = map->GetWorldRight() - halfScreen;
    if (minViewX > maxViewX) {
        return 0.0f;
    }
    return minViewX;
}

std::vector<std::string> CoinFramePaths() {
    return {
        AssetPaths::Image("homescreen/coins1.png"),
        AssetPaths::Image("homescreen/coins2.png"),
        AssetPaths::Image("homescreen/coins3.png"),
        AssetPaths::Image("homescreen/coins4.png"),
        AssetPaths::Image("homescreen/coins5.png"),
        AssetPaths::Image("homescreen/coins6.png"),
        AssetPaths::Image("homescreen/coins7.png"),
    };
}

} // namespace

void App::ActivateNearbyEnemies() {
    if (!m_Mario) return;

    for (auto& pending : m_PendingEnemySpawns) {
        if (pending.activated) continue;
        if (std::abs(pending.position.x - m_Mario->m_Transform.translation.x) > ENEMY_SPAWN_RANGE_X) {
            continue;
        }

        m_Enemies.push_back(std::make_unique<Enemy>(pending.position.x, pending.position.y));
        pending.activated = true;
    }
}

void App::StartGoalSequence() {
    if (!m_Mario || !g_MapManager || m_GoalSequenceStage != GoalSequenceStage::None) return;

    m_GoalSequenceStage = GoalSequenceStage::Sliding;
    m_GoalSequenceTimer = 0.0f;
    m_GoalFlagMarioOffsetY = 0.0f;
    m_GoalCelebrationPlayed = false;
    m_Fireballs.clear();
    m_FireballCooldown = 0.0f;
    AddScore(static_cast<int>(std::max(0.0f, m_LevelTimer)) * 10);
    StopMusic(250);
    PlaySfx(m_Audio.flagpole.get());
    PlaySfx(m_Audio.worldClear.get());

    if (m_CastleObject != nullptr && m_CastleImage != nullptr) {
        const float targetHeight = g_MapManager->GetTileSize() * CASTLE_TARGET_HEIGHT_TILES;
        const glm::vec2 castleSize = m_CastleImage->GetSize();
        float castleScale = 1.0f;
        float castleWidth = targetHeight;
        if (castleSize.y > 0.0f) {
            castleScale = targetHeight / castleSize.y;
            castleWidth = castleSize.x * castleScale;
        }

        const float minCastleCenter = g_MapManager->GetWorldLeft() + castleWidth * 0.5f;
        const float maxCastleCenter = g_MapManager->GetWorldRight()
            - castleWidth * 0.5f
            - g_MapManager->GetTileSize() * CASTLE_END_INSET_TILES;
        const float desiredCastleX = g_MapManager->GetGoalX() + g_MapManager->GetTileSize() * CASTLE_OFFSET_TILES;
        const float castleX = std::clamp(desiredCastleX, minCastleCenter, maxCastleCenter);
        const float castleGroundY = g_MapManager->GetGoalGroundY() - g_MapManager->GetTileSize();
        const float castleY = castleGroundY + targetHeight * 0.5f;

        m_CastleDoorX = castleX;
        m_CastleObject->m_Transform.translation = { castleX, castleY };
        m_CastleObject->m_Transform.scale = { castleScale, castleScale };
    } else {
        m_CastleDoorX = g_MapManager->GetWorldRight() - g_MapManager->GetTileSize() * 2.0f;
    }

    const float touchY = std::clamp(
        m_Mario->m_Transform.translation.y,
        g_MapManager->GetFlagBottomY(),
        g_MapManager->GetFlagTopY()
    );
    g_MapManager->SetFlagY(touchY);

    m_Mario->StartGoalSequence(
        g_MapManager->GetGoalX(),
        g_MapManager->GetFlagX(),
        g_MapManager->GetFlagBottomY(),
        g_MapManager->GetGoalGroundY(),
        touchY
    );
    m_GoalFlagMarioOffsetY = touchY - m_Mario->m_Transform.translation.y;
}

void App::UpdateGoalSequence(float dt) {
    if (!m_Mario || !g_MapManager || m_GoalSequenceStage == GoalSequenceStage::None) return;

    if (m_GoalSequenceStage == GoalSequenceStage::Sliding) {
        m_Mario->Update();
        const float nextFlagY = m_Mario->m_Transform.translation.y + m_GoalFlagMarioOffsetY;
        g_MapManager->SetFlagY(nextFlagY);
        if (m_Mario->IsGoalSequenceFinished()) {
            g_MapManager->SetFlagY(g_MapManager->GetFlagBottomY());
            m_GoalSequenceStage = GoalSequenceStage::PlayerControl;
        }
    } else if (m_GoalSequenceStage == GoalSequenceStage::PlayerControl) {
        if (m_Mario->m_Transform.translation.x >= m_CastleDoorX) {
            m_Mario->m_Transform.translation.x = m_CastleDoorX;
            m_Mario->SetVisible(false);
            m_GoalSequenceStage = GoalSequenceStage::Entering;
            m_GoalSequenceTimer = GOAL_FINISH_DELAY;
            if (!m_GoalCelebrationPlayed) {
                m_GoalCelebrationPlayed = true;
                PlaySfx(m_Audio.pipe.get());
                PlaySfx(m_Audio.stageClear.get());
                PlaySfx(m_Audio.fireworks.get());
            }
        }
    } else if (m_GoalSequenceStage == GoalSequenceStage::Entering) {
        m_GoalSequenceTimer = std::max(0.0f, m_GoalSequenceTimer - dt);
        if (m_GoalSequenceTimer <= 0.0f) {
            m_GoalSequenceStage = GoalSequenceStage::Finished;
            m_CurrentState = State::END;
        }
    }
}

void App::InitializeAudio() {
    if (m_Audio.groundTheme != nullptr) return;

    m_Audio.groundTheme = std::make_unique<Util::BGM>(AssetPaths::Sound("01. Ground Theme.mp3"));
    m_Audio.oneUp = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_1-up.wav"));
    m_Audio.bowserFalls = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_bowserfalls.wav"));
    m_Audio.bowserFire = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_bowserfire.wav"));
    m_Audio.breakBlock = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_breakblock.wav"));
    m_Audio.bump = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_bump.wav"));
    m_Audio.coin = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_coin.wav"));
    m_Audio.fireball = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_fireball.wav"));
    m_Audio.fireworks = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_fireworks.wav"));
    m_Audio.flagpole = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_flagpole.wav"));
    m_Audio.gameOver = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_gameover.wav"));
    m_Audio.jumpSmall = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_jump-small.wav"));
    m_Audio.jumpSuper = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_jump-super.wav"));
    m_Audio.kick = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_kick.wav"));
    m_Audio.marioDie = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_mariodie.wav"));
    m_Audio.pause = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_pause.wav"));
    m_Audio.pipe = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_pipe.wav"));
    m_Audio.powerUp = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_powerup.wav"));
    m_Audio.powerUpAppears = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_powerup_appears.wav"));
    m_Audio.stageClear = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_stage_clear.wav"));
    m_Audio.stomp = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_stomp.wav"));
    m_Audio.vine = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_vine.wav"));
    m_Audio.warning = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_warning.wav"));
    m_Audio.worldClear = std::make_unique<Util::SFX>(AssetPaths::Sound("smb_world_clear.wav"));

    if (m_Audio.groundTheme != nullptr) {
        m_Audio.groundTheme->SetVolume(48);
    }

    const std::vector<Util::SFX*> effects = {
        m_Audio.oneUp.get(),
        m_Audio.bowserFalls.get(),
        m_Audio.bowserFire.get(),
        m_Audio.breakBlock.get(),
        m_Audio.bump.get(),
        m_Audio.coin.get(),
        m_Audio.fireball.get(),
        m_Audio.fireworks.get(),
        m_Audio.flagpole.get(),
        m_Audio.gameOver.get(),
        m_Audio.jumpSmall.get(),
        m_Audio.jumpSuper.get(),
        m_Audio.kick.get(),
        m_Audio.marioDie.get(),
        m_Audio.pause.get(),
        m_Audio.pipe.get(),
        m_Audio.powerUp.get(),
        m_Audio.powerUpAppears.get(),
        m_Audio.stageClear.get(),
        m_Audio.stomp.get(),
        m_Audio.vine.get(),
        m_Audio.warning.get(),
        m_Audio.worldClear.get()
    };

    for (auto* effect : effects) {
        if (effect != nullptr) {
            effect->SetVolume(72);
        }
    }

    if (m_Audio.fireworks != nullptr) {
        m_Audio.fireworks->SetVolume(60);
    }
    if (m_Audio.warning != nullptr) {
        m_Audio.warning->SetVolume(64);
    }
}

void App::PlaySfx(Util::SFX* sfx, int loop, int duration) {
    if (sfx != nullptr) {
        sfx->Play(loop, duration);
    }
}

void App::PlayTitleMusic() {
    InitializeAudio();
    if (m_Audio.groundTheme == nullptr || m_ActiveMusic == MusicTrack::GroundTheme) return;
    m_Audio.groundTheme->Play(-1);
    m_ActiveMusic = MusicTrack::GroundTheme;
}

void App::PlayGameplayMusic(bool restart) {
    InitializeAudio();
    if (m_Audio.groundTheme == nullptr) return;
    if (!restart && m_ActiveMusic == MusicTrack::GroundTheme) return;
    m_Audio.groundTheme->Play(-1);
    m_ActiveMusic = MusicTrack::GroundTheme;
}

void App::StopMusic(int fadeMs) {
    if (fadeMs > 0) {
        Mix_FadeOutMusic(fadeMs);
    } else {
        Mix_HaltMusic();
    }
    m_ActiveMusic = MusicTrack::None;
}

void App::PauseMusic() {
    if (m_ActiveMusic != MusicTrack::None) {
        Mix_PauseMusic();
    }
}

void App::ResumeMusic() {
    if (m_ActiveMusic != MusicTrack::None && Mix_PausedMusic() == 1) {
        Mix_ResumeMusic();
    }
}

void App::TogglePause() {
    if (m_ScreenState == ScreenState::Gameplay) {
        m_ScreenState = ScreenState::Paused;
        m_StatusMessageText.position = { CenteredTextX("PAUSED", STATUS_MESSAGE_SCALE, 3.0f), -8.0f };
        m_StatusMessageText.layoutDirty = true;
        SetSpriteText(m_StatusMessageText, "PAUSED");
        PlaySfx(m_Audio.pause.get());
        PauseMusic();
        return;
    }

    if (m_ScreenState == ScreenState::Paused) {
        m_ScreenState = ScreenState::Gameplay;
        PlaySfx(m_Audio.pause.get());
        ResumeMusic();
    }
}

void App::InitializeAnimatedSprite(AnimatedSprite& sprite,
                                   const std::vector<std::string>& framePaths,
                                   const glm::vec2& position,
                                   const glm::vec2& scale,
                                   float zIndex,
                                   float frameDuration) {
    sprite.frames.clear();
    for (const auto& framePath : framePaths) {
        sprite.frames.push_back(std::make_shared<Util::Image>(framePath));
    }

    auto object = std::make_shared<Util::GameObject>();
    if (!sprite.frames.empty()) {
        object->SetDrawable(sprite.frames.front());
    }
    object->m_Transform.translation = position;
    object->m_Transform.scale = scale;
    object->SetZIndex(zIndex);

    sprite.object = object;
    sprite.frameDuration = frameDuration;
    sprite.timer = 0.0f;
    sprite.frameIndex = 0;
}

void App::AdvanceAnimatedSprite(AnimatedSprite& sprite, float dt) {
    if (sprite.object == nullptr || sprite.frames.size() <= 1) return;

    sprite.timer += dt;
    while (sprite.timer >= sprite.frameDuration) {
        sprite.timer -= sprite.frameDuration;
        sprite.frameIndex = (sprite.frameIndex + 1) % sprite.frames.size();
        sprite.object->SetDrawable(sprite.frames[sprite.frameIndex]);
    }
}

void App::ConfigureSpriteText(SpriteText& spriteText,
                              const glm::vec2& position,
                              const glm::vec2& scale,
                              float spacing,
                              float lineHeight,
                              float zIndex) {
    spriteText.position = position;
    spriteText.scale = scale;
    spriteText.spacing = spacing;
    spriteText.lineHeight = lineHeight;
    spriteText.zIndex = zIndex;
    spriteText.layoutDirty = true;
}

std::string App::GetFontSpritePath(char character) const {
    switch (character) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return AssetPaths::Image(std::string("Font/") + character + ".png");
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
    case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
    case 'V': case 'W': case 'X': case 'Y': case 'Z':
        return AssetPaths::Image(std::string("Font/") + character + ".png");
    case '-':
        return AssetPaths::Image("Font/strip.png");
    case '.':
        return AssetPaths::Image("Font/dot.png");
    case '!':
        return AssetPaths::Image("Font/exclamation.png");
    case 'x':
        return AssetPaths::Image("Font/times.png");
    case '~':
        return AssetPaths::Image("Font/circled_c.png");
    default:
        return "";
    }
}

void App::LayoutSpriteText(SpriteText& spriteText) {
    const float advanceX = FONT_GLYPH_SIZE * spriteText.scale.x + spriteText.spacing;
    const float advanceY = (spriteText.lineHeight > 0.0f)
        ? spriteText.lineHeight
        : FONT_GLYPH_SIZE * spriteText.scale.y + spriteText.spacing;

    float currentX = spriteText.position.x;
    float currentY = spriteText.position.y;
    std::size_t glyphIndex = 0;

    for (char rawCharacter : spriteText.content) {
        if (rawCharacter == '\n') {
            currentX = spriteText.position.x;
            currentY -= advanceY;
            continue;
        }

        if (rawCharacter == ' ') {
            currentX += advanceX;
            continue;
        }

        char lookupCharacter = rawCharacter;
        if (lookupCharacter != 'x' && lookupCharacter != '~') {
            lookupCharacter = static_cast<char>(
                std::toupper(static_cast<unsigned char>(lookupCharacter))
            );
        }
        const std::string spritePath = GetFontSpritePath(lookupCharacter);
        if (spritePath.empty()) {
            currentX += advanceX;
            continue;
        }

        if (glyphIndex >= spriteText.glyphs.size()) {
            break;
        }

        auto& glyph = spriteText.glyphs[glyphIndex++];
        glyph->m_Transform.translation = {
            currentX + FONT_GLYPH_SIZE * spriteText.scale.x * 0.5f,
            currentY
        };
        glyph->m_Transform.scale = spriteText.scale;
        currentX += advanceX;
    }

    spriteText.layoutDirty = false;
}

void App::SetSpriteText(SpriteText& spriteText, const std::string& text) {
    if (spriteText.content == text) {
        if (spriteText.layoutDirty) {
            LayoutSpriteText(spriteText);
        }
        return;
    }

    spriteText.glyphs.clear();
    spriteText.content = text;

    for (char rawCharacter : spriteText.content) {
        if (rawCharacter == '\n' || rawCharacter == ' ') {
            continue;
        }

        char lookupCharacter = rawCharacter;
        if (lookupCharacter != 'x' && lookupCharacter != '~') {
            lookupCharacter = static_cast<char>(
                std::toupper(static_cast<unsigned char>(lookupCharacter))
            );
        }
        const std::string spritePath = GetFontSpritePath(lookupCharacter);
        if (spritePath.empty()) {
            continue;
        }

        auto glyph = std::make_shared<Util::GameObject>();
        glyph->SetDrawable(std::make_shared<Util::Image>(spritePath));
        glyph->SetZIndex(spriteText.zIndex);
        spriteText.glyphs.push_back(glyph);
    }

    spriteText.layoutDirty = true;
    LayoutSpriteText(spriteText);
}

void App::DrawSpriteText(const SpriteText& spriteText) const {
    for (const auto& glyph : spriteText.glyphs) {
        DrawUiObject(glyph);
    }
}

void App::DrawUiObject(const std::shared_ptr<Util::GameObject>& object) const {
    if (object != nullptr) {
        object->Draw();
    }
}

void App::InitializeUi() {
    InitializeAnimatedSprite(
        m_HudCoinIcon,
        CoinFramePaths(),
        { -116.0f, 304.0f },
        { 3.0f, 3.0f },
        UI_Z,
        COIN_FRAME_DURATION
    );
    InitializeAnimatedSprite(
        m_TitleCoinIcon,
        CoinFramePaths(),
        { -116.0f, 304.0f },
        { 3.0f, 3.0f },
        UI_Z,
        COIN_FRAME_DURATION
    );

    m_TitleLogoImage = std::make_shared<Util::GameObject>();
    m_TitleLogoImage->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("title.png")));
    m_TitleLogoImage->m_Transform.translation = { 0.0f, 96.0f };
    m_TitleLogoImage->m_Transform.scale = { TITLE_LOGO_IMAGE_SCALE, TITLE_LOGO_IMAGE_SCALE };
    m_TitleLogoImage->SetZIndex(UI_Z - 1.0f);

    m_TitleNintendoText = std::make_shared<Util::GameObject>();
    m_TitleNintendoText->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("nintendo_text.png")));
    m_TitleNintendoText->m_Transform.translation = { 55.0f, -75.0f };
    m_TitleNintendoText->m_Transform.scale = { TITLE_COPYRIGHT_SCALE, TITLE_COPYRIGHT_SCALE };
    m_TitleNintendoText->SetZIndex(UI_Z - 1.0f);

    m_TitleCloudLeft = std::make_shared<Util::GameObject>();
    m_TitleCloudLeft->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("Clouds.png")));
    m_TitleCloudLeft->m_Transform.translation = { -236.0f, 222.0f };
    m_TitleCloudLeft->m_Transform.scale = { 4.0f, 4.0f };
    m_TitleCloudLeft->SetZIndex(TITLE_BG_Z);

    m_TitleCloudRight = std::make_shared<Util::GameObject>();
    m_TitleCloudRight->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("Clouds.png")));
    m_TitleCloudRight->m_Transform.translation = { 226.0f, 186.0f };
    m_TitleCloudRight->m_Transform.scale = { 4.0f, 4.0f };
    m_TitleCloudRight->SetZIndex(TITLE_BG_Z);

    m_TitleMountain = std::make_shared<Util::GameObject>();
    m_TitleMountain->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("mountains.png")));
    m_TitleMountain->m_Transform.translation = { 210.0f, -266.0f };
    m_TitleMountain->m_Transform.scale = { 0.24f, 0.24f };
    m_TitleMountain->SetZIndex(TITLE_DECOR_Z);

    m_TitleBushLeft = std::make_shared<Util::GameObject>();
    m_TitleBushLeft->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("Bush.png")));
    m_TitleBushLeft->m_Transform.translation = { -246.0f, -282.0f };
    m_TitleBushLeft->m_Transform.scale = { 4.0f, 4.0f };
    m_TitleBushLeft->SetZIndex(TITLE_DECOR_Z + 1.0f);

    m_TitleBush = std::make_shared<Util::GameObject>();
    m_TitleBush->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("Bush.png")));
    m_TitleBush->m_Transform.translation = { 286.0f, -286.0f };
    m_TitleBush->m_Transform.scale = { 4.0f, 4.0f };
    m_TitleBush->SetZIndex(TITLE_DECOR_Z + 1.0f);

    m_TitleMario = std::make_shared<Util::GameObject>();
    m_TitleMario->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("Character/MarioIdle.png")));
    m_TitleMario->m_Transform.translation = { -182.0f, -262.0f };
    m_TitleMario->m_Transform.scale = { 4.0f, 4.0f };
    m_TitleMario->SetZIndex(TITLE_DECOR_Z + 2.0f);

    m_TitleCursor = std::make_shared<Util::GameObject>();
    m_TitleCursor->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("homescreen/cursor.png")));
    m_TitleCursor->m_Transform.scale = { 4.0f, 4.0f };
    m_TitleCursor->SetZIndex(UI_Z);

    m_IntroMario = std::make_shared<Util::GameObject>();
    m_IntroMario->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("Character/MarioIdle.png")));
    m_IntroMario->m_Transform.translation = { -75.0f, -24.0f };
    m_IntroMario->m_Transform.scale = { 5.0f, 5.0f };
    m_IntroMario->SetZIndex(UI_Z);

    ConfigureSpriteText(m_HudMarioLabel, { -358.0f, 326.0f }, { HUD_TEXT_SCALE, HUD_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
    ConfigureSpriteText(m_HudScoreValue, { -358.0f, 288.0f }, { HUD_TEXT_SCALE, HUD_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
    ConfigureSpriteText(m_HudWorldLabel, { 74.0f, 326.0f }, { HUD_TEXT_SCALE, HUD_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
    ConfigureSpriteText(m_HudWorldValue, { 116.0f, 288.0f }, { HUD_TEXT_SCALE, HUD_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
    ConfigureSpriteText(m_HudTimeLabel, { 244.0f, 326.0f }, { HUD_TEXT_SCALE, HUD_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
    ConfigureSpriteText(m_HudTimeValue, { 274.0f, 288.0f }, { HUD_TEXT_SCALE, HUD_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
    ConfigureSpriteText(m_HudCoinValue, { -74.0f, 294.0f }, { HUD_TEXT_SCALE, HUD_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);

    ConfigureSpriteText(m_TitleOption1, { -171.0f, -128.0f }, { TITLE_TEXT_SCALE, TITLE_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
    ConfigureSpriteText(m_TitleOption2, { -171.0f, -174.0f }, { TITLE_TEXT_SCALE, TITLE_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
    ConfigureSpriteText(m_TitleTopScore, { -133.0f, -226.0f }, { TITLE_TEXT_SCALE, TITLE_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);

    ConfigureSpriteText(m_IntroWorldText, { -116.0f, 86.0f }, { INTRO_TEXT_SCALE, INTRO_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
    ConfigureSpriteText(m_IntroLivesText, { 6.0f, -24.0f }, { INTRO_TEXT_SCALE, INTRO_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
    ConfigureSpriteText(m_StatusMessageText, { 0.0f, -8.0f }, { STATUS_MESSAGE_SCALE, STATUS_MESSAGE_SCALE }, 3.0f, 0.0f, UI_Z);

    m_TitleGroundTiles.clear();
    const float titleGroundScale = 4.0f;
    const float tileSize = 16.0f * titleGroundScale;
    const float startX = -static_cast<float>(WINDOW_WIDTH) * 0.5f + tileSize * 0.5f;
    const float groundY = -static_cast<float>(WINDOW_HEIGHT) * 0.5f + tileSize * 0.5f - 2.0f;
    const int tileCount = static_cast<int>(std::ceil(static_cast<float>(WINDOW_WIDTH) / tileSize)) + 2;
    for (int i = 0; i < tileCount; ++i) {
        auto tile = std::make_shared<Util::GameObject>();
        tile->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("Tiles/Ground.png")));
        tile->m_Transform.translation = { startX + tileSize * static_cast<float>(i), groundY };
        tile->m_Transform.scale = { titleGroundScale, titleGroundScale };
        tile->SetZIndex(TITLE_GROUND_Z);
        m_TitleGroundTiles.push_back(tile);
    }

    RefreshHudText();
}

void App::RefreshHudText() {
    m_DisplayedLevelTime = DisplayLevelTime(m_LevelTimer);
    SetSpriteText(m_HudMarioLabel, "MARIO");
    SetSpriteText(m_HudScoreValue, PadNumber(m_Score, 6));
    SetSpriteText(m_HudWorldLabel, "WORLD");
    SetSpriteText(m_HudWorldValue, WorldLabel(m_World, m_Level));
    SetSpriteText(m_HudTimeLabel, "TIME");
    SetSpriteText(m_HudTimeValue, PadNumber(m_DisplayedLevelTime, 3));
    SetSpriteText(m_HudCoinValue, "x" + PadNumber(m_Coins, 2));

    SetSpriteText(m_TitleOption1, "1 PLAYER GAME");
    SetSpriteText(m_TitleOption2, "2 PLAYER GAME");
    SetSpriteText(m_TitleTopScore, "TOP-" + PadNumber(m_TopScore, 6));

    SetSpriteText(m_IntroWorldText, "WORLD " + WorldLabel(m_World, m_Level));
    SetSpriteText(m_IntroLivesText, "x " + std::to_string(std::max(0, m_Lives)));
}

void App::AddScore(int points) {
    if (points <= 0) return;
    m_Score += points;
    m_TopScore = std::max(m_TopScore, m_Score);
    RefreshHudText();
}

void App::AwardPoints(int points, const glm::vec2& worldPosition, const std::string& popupText) {
    if (points <= 0 && popupText.empty()) return;
    if (points > 0) {
        AddScore(points);
    }
    const std::string text = popupText.empty() ? std::to_string(points) : popupText;
    if (!text.empty()) {
        SpawnFloatingText(text, worldPosition);
    }
}

void App::SpawnFloatingText(const std::string& text, const glm::vec2& worldPosition) {
    FloatingText floatingText;
    floatingText.value = text;
    floatingText.worldPosition = worldPosition;
    floatingText.lifetime = 0.85f;
    floatingText.riseSpeed = 44.0f;
    ConfigureSpriteText(
        floatingText.text,
        { worldPosition.x - m_ViewX, worldPosition.y + 28.0f },
        { 2.5f, 2.5f },
        2.0f,
        0.0f,
        UI_Z - 1.0f
    );
    SetSpriteText(floatingText.text, text);
    m_FloatingTexts.push_back(std::move(floatingText));
}

void App::UpdateFloatingTexts(float dt) {
    for (auto& floatingText : m_FloatingTexts) {
        floatingText.lifetime = std::max(0.0f, floatingText.lifetime - dt);
        floatingText.worldPosition.y += floatingText.riseSpeed * dt;
        floatingText.text.position = { floatingText.worldPosition.x - m_ViewX, floatingText.worldPosition.y };
        floatingText.text.layoutDirty = true;
        SetSpriteText(floatingText.text, floatingText.value);
    }

    m_FloatingTexts.erase(
        std::remove_if(
            m_FloatingTexts.begin(),
            m_FloatingTexts.end(),
            [](const FloatingText& floatingText) { return floatingText.lifetime <= 0.0f; }
        ),
        m_FloatingTexts.end()
    );
}

void App::DrawFloatingTexts() {
    for (const auto& floatingText : m_FloatingTexts) {
        DrawSpriteText(floatingText.text);
    }
}

void App::StartLevelIntro(float duration) {
    m_LevelIntroTimer = duration;
    m_ScreenState = ScreenState::LevelIntro;
    m_GoalCelebrationPlayed = false;
    PlaySfx(m_Audio.vine.get());
    RefreshHudText();
}

void App::BeginStatusMessage(const std::string& message, float duration, StatusMessageAction action) {
    m_StatusMessageTimer = duration;
    m_StatusMessageAction = action;
    m_ScreenState = ScreenState::StatusMessage;
    m_StatusMessageText.position = { CenteredTextX(message, STATUS_MESSAGE_SCALE, 3.0f), -8.0f };
    m_StatusMessageText.layoutDirty = true;
    SetSpriteText(m_StatusMessageText, message);
    RefreshHudText();
}

void App::ResetGameSession() {
    m_Score = 0;
    m_Coins = 0;
    m_Lives = STARTING_LIVES;
    m_World = 1;
    m_Level = 1;
    m_StatusMessageTimer = 0.0f;
    m_StatusMessageAction = StatusMessageAction::None;
    m_DeathWasTimeout = false;
    m_TitleBlinkTimer = 0.0f;
    m_TitleSelection = 0;
    m_LowTimeWarningPlayed = false;
    m_GoalCelebrationPlayed = false;
}

void App::LoadLevel() {
    g_MapManager = std::make_unique<MapManager>();
    m_Mario = std::make_unique<Mario>();
    m_Enemies.clear();
    m_PendingEnemySpawns.clear();
    m_Fireballs.clear();
    m_Pickups.clear();
    m_Debris.clear();
    m_FloatingTexts.clear();
    m_FireballCooldown = 0.0f;
    m_GoalSequenceStage = GoalSequenceStage::None;
    m_GoalSequenceTimer = 0.0f;
    m_CastleDoorX = 0.0f;
    m_GoalFlagMarioOffsetY = 0.0f;
    m_Score = 0;
    m_Coins = 0;
    m_LevelTimer = STARTING_TIMER;
    m_DisplayedLevelTime = STARTING_TIMER;
    m_LevelIntroTimer = 0.0f;
    m_StatusMessageTimer = 0.0f;
    m_StatusMessageAction = StatusMessageAction::None;
    m_WasMarioDead = false;
    m_DeathWasTimeout = false;
    m_LowTimeWarningPlayed = false;
    m_GoalCelebrationPlayed = false;
    m_SkyColor = Util::Color(
        static_cast<unsigned char>(SKY_BLUE_R),
        static_cast<unsigned char>(SKY_BLUE_G),
        static_cast<unsigned char>(SKY_BLUE_B),
        255
    );

    std::vector<glm::vec2> enemySpawns;
    bool foundSpawn = convert_sketch(
        AssetPaths::Image("LevelSketch0.png"),
        *g_MapManager,
        *m_Mario,
        m_SkyColor,
        &enemySpawns
    );

    glClearColor(m_SkyColor.r / 255.0f, m_SkyColor.g / 255.0f, m_SkyColor.b / 255.0f, 1.0f);

    for (const auto& spawn : enemySpawns) {
        m_PendingEnemySpawns.push_back({ spawn, false });
    }

    if (!foundSpawn) {
        m_Mario->m_Transform.translation = { 0.0f, -200.0f };
    }
    m_Mario->SetSpawnPosition(m_Mario->m_Transform.translation);

    m_ViewX = 0.0f;
    m_CastleObject = std::make_shared<Util::GameObject>();
    m_CastleImage = std::make_shared<Util::Image>(AssetPaths::Image("castle1.png"));
    m_CastleObject->SetDrawable(m_CastleImage);
    m_CastleObject->SetZIndex(5.0f);
    if (g_MapManager && g_MapManager->HasGoal() && m_CastleImage != nullptr) {
        const float targetHeight = g_MapManager->GetTileSize() * CASTLE_TARGET_HEIGHT_TILES;
        const glm::vec2 castleSize = m_CastleImage->GetSize();
        float castleScale = 1.0f;
        float castleWidth = targetHeight;
        if (castleSize.y > 0.0f) {
            castleScale = targetHeight / castleSize.y;
            castleWidth = castleSize.x * castleScale;
        }

        const float minCastleCenter = g_MapManager->GetWorldLeft() + castleWidth * 0.5f;
        const float maxCastleCenter = g_MapManager->GetWorldRight()
            - castleWidth * 0.5f
            - g_MapManager->GetTileSize() * CASTLE_END_INSET_TILES;
        const float desiredCastleX = g_MapManager->GetGoalX() + g_MapManager->GetTileSize() * CASTLE_OFFSET_TILES;
        const float castleX = std::clamp(desiredCastleX, minCastleCenter, maxCastleCenter);
        const float castleGroundY = g_MapManager->GetGoalGroundY() - g_MapManager->GetTileSize();
        const float castleY = castleGroundY + targetHeight * 0.5f;

        m_CastleObject->m_Transform.translation = { castleX, castleY };
        m_CastleObject->m_Transform.scale = { castleScale, castleScale };
        m_CastleDoorX = castleX;
    }

    InitializeUi();
}

void App::HandleMarioDeath() {
    if (!m_Mario || !m_Mario->IsDeathSequenceFinished()) return;

    if (m_DeathWasTimeout) {
        BeginStatusMessage(
            "TIME UP",
            STATUS_MESSAGE_DURATION,
            (m_Lives > 0) ? StatusMessageAction::ReloadLevel : StatusMessageAction::ShowGameOver
        );
        return;
    }

    if (m_Lives > 0) {
        LoadLevel();
        StartLevelIntro(LEVEL_INTRO_DURATION);
        return;
    }

    PlaySfx(m_Audio.gameOver.get());
    BeginStatusMessage("GAME OVER", STATUS_MESSAGE_DURATION, StatusMessageAction::RestartToTitle);
}

void App::Start() {
    LOG_TRACE("Start");

    InitializeAudio();
    ResetGameSession();
    LoadLevel();

    LOG_TRACE("Map size = {} x {}", g_MapManager->GetWidth(), g_MapManager->GetHeight());
    LOG_TRACE("Mario start pos = {}, {}",
        m_Mario->m_Transform.translation.x,
        m_Mario->m_Transform.translation.y);

    m_ScreenState = ScreenState::Title;
    PlayTitleMusic();
    m_CurrentState = State::UPDATE;
}

void App::DrawHud() {
    if (m_HudCoinIcon.object != nullptr) {
        m_HudCoinIcon.object->m_Transform.translation = {
            m_HudCoinValue.position.x - 20.0f,
            m_HudCoinValue.position.y + 2.0f
        };
    }
    DrawSpriteText(m_HudMarioLabel);
    DrawSpriteText(m_HudScoreValue);
    DrawSpriteText(m_HudWorldLabel);
    DrawSpriteText(m_HudWorldValue);
    DrawSpriteText(m_HudTimeLabel);
    DrawSpriteText(m_HudTimeValue);
    DrawUiObject(m_HudCoinIcon.object);
    DrawSpriteText(m_HudCoinValue);
}

void App::RenderTitleScreen() {
    glClearColor(
        m_SkyColor.r / 255.0f,
        m_SkyColor.g / 255.0f,
        m_SkyColor.b / 255.0f,
        1.0f
    );
    glClear(GL_COLOR_BUFFER_BIT);

    if (g_MapManager != nullptr) {
        g_MapManager->Draw(GetLeftEdgeViewX(g_MapManager.get()));
    }

    if (m_Mario != nullptr && g_MapManager != nullptr) {
        const float titleViewX = GetLeftEdgeViewX(g_MapManager.get());
        const glm::vec2 oldPos = m_Mario->m_Transform.translation;
        m_Mario->m_Transform.translation.x -= titleViewX;
        m_Mario->m_Transform.translation.y += m_Mario->GetRenderOffsetY();
        m_Mario->Draw();
        m_Mario->m_Transform.translation = oldPos;
    }

    if (m_TitleCoinIcon.object != nullptr) {
        m_TitleCoinIcon.object->m_Transform.translation = {
            m_HudCoinValue.position.x - 20.0f,
            m_HudCoinValue.position.y + 2.0f
        };
    }

    DrawSpriteText(m_HudMarioLabel);
    DrawSpriteText(m_HudScoreValue);
    DrawSpriteText(m_HudWorldLabel);
    DrawSpriteText(m_HudWorldValue);
    DrawSpriteText(m_HudTimeLabel);
    DrawUiObject(m_TitleCoinIcon.object);
    DrawSpriteText(m_HudCoinValue);
    DrawUiObject(m_TitleLogoImage);
    DrawSpriteText(m_TitleOption1);
    DrawSpriteText(m_TitleOption2);
    DrawSpriteText(m_TitleTopScore);
    DrawUiObject(m_TitleNintendoText);
    DrawUiObject(m_TitleCursor);
}

void App::RenderLevelIntro() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (m_HudCoinIcon.object != nullptr) {
        m_HudCoinIcon.object->m_Transform.translation = {
            m_HudCoinValue.position.x - 20.0f,
            m_HudCoinValue.position.y + 2.0f
        };
    }
    DrawSpriteText(m_HudMarioLabel);
    DrawSpriteText(m_HudScoreValue);
    DrawSpriteText(m_HudWorldLabel);
    DrawSpriteText(m_HudWorldValue);
    DrawSpriteText(m_HudTimeLabel);
    DrawUiObject(m_HudCoinIcon.object);
    DrawSpriteText(m_HudCoinValue);
    DrawSpriteText(m_IntroWorldText);
    DrawUiObject(m_IntroMario);
    DrawSpriteText(m_IntroLivesText);
}

void App::RenderStatusMessage() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    DrawHud();
    DrawSpriteText(m_StatusMessageText);
}

void App::RenderPaused() {
    RenderGameplay();
    DrawSpriteText(m_StatusMessageText);
}

void App::RenderGameplay() {
    glClearColor(m_SkyColor.r / 255.0f, m_SkyColor.g / 255.0f, m_SkyColor.b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (g_MapManager) {
        g_MapManager->Draw(m_ViewX);
    }

    if (m_CastleObject != nullptr &&
        g_MapManager != nullptr) {
        const glm::vec2 oldPos = m_CastleObject->m_Transform.translation;
        m_CastleObject->m_Transform.translation.x -= m_ViewX;
        m_CastleObject->Draw();
        m_CastleObject->m_Transform.translation = oldPos;
    }
    if (m_Mario) {
        const glm::vec2 oldPos = m_Mario->m_Transform.translation;
        m_Mario->m_Transform.translation.x -= m_ViewX;
        m_Mario->m_Transform.translation.y += m_Mario->GetRenderOffsetY();
        m_Mario->Draw();
        m_Mario->m_Transform.translation = oldPos;
    }
    for (auto& enemy : m_Enemies) {
        const glm::vec2 oldPos = enemy->m_Transform.translation;
        enemy->m_Transform.translation.x -= m_ViewX;
        enemy->Draw();
        enemy->m_Transform.translation = oldPos;
    }
    for (auto& fireball : m_Fireballs) {
        const glm::vec2 oldPos = fireball->m_Transform.translation;
        fireball->m_Transform.translation.x -= m_ViewX;
        fireball->Draw();
        fireball->m_Transform.translation = oldPos;
    }
    for (auto& pickup : m_Pickups) {
        const glm::vec2 oldPos = pickup->m_Transform.translation;
        pickup->m_Transform.translation.x -= m_ViewX;
        pickup->Draw();
        pickup->m_Transform.translation = oldPos;
    }
    for (auto& debris : m_Debris) {
        const glm::vec2 oldPos = debris->m_Transform.translation;
        debris->m_Transform.translation.x -= m_ViewX;
        debris->Draw();
        debris->m_Transform.translation = oldPos;
    }

    DrawFloatingTexts();

    DrawHud();
}

void App::UpdateTitleScreen(float dt) {
    AdvanceAnimatedSprite(m_TitleCoinIcon, dt);
    m_TitleBlinkTimer += dt;

    if (Util::Input::IsKeyPressed(Util::Keycode::UP) ||
        Util::Input::IsKeyPressed(Util::Keycode::DOWN) ||
        Util::Input::IsKeyPressed(Util::Keycode::W) ||
        Util::Input::IsKeyPressed(Util::Keycode::S)) {
        m_TitleSelection = 1 - m_TitleSelection;
        PlaySfx(m_Audio.bump.get());
    }

    const float cursorY = (m_TitleSelection == 0) ? -128.0f : -174.0f;
    if (m_TitleCursor != nullptr) {
        m_TitleCursor->m_Transform.translation = { -216.0f, cursorY };
        m_TitleCursor->SetVisible(std::fmod(m_TitleBlinkTimer, TITLE_CURSOR_BLINK_DURATION * 2.0f) < TITLE_CURSOR_BLINK_DURATION);
    }

    if (Util::Input::IsKeyPressed(Util::Keycode::RETURN) ||
        Util::Input::IsKeyPressed(Util::Keycode::SPACE)) {
        PlaySfx(m_Audio.pause.get());
        StartLevelIntro(LEVEL_INTRO_DURATION);
    }

    RenderTitleScreen();
}

void App::UpdateLevelIntro(float dt) {
    AdvanceAnimatedSprite(m_HudCoinIcon, dt);
    m_LevelIntroTimer = std::max(0.0f, m_LevelIntroTimer - dt);
    if (m_LevelIntroTimer <= 0.0f) {
        PlayGameplayMusic(true);
        m_ScreenState = ScreenState::Gameplay;
    }

    RenderLevelIntro();
}

void App::UpdatePaused(float) {
    RenderPaused();
}

void App::UpdateStatusMessage(float dt) {
    AdvanceAnimatedSprite(m_HudCoinIcon, dt);
    m_StatusMessageTimer = std::max(0.0f, m_StatusMessageTimer - dt);
    if (m_StatusMessageTimer <= 0.0f) {
        const StatusMessageAction action = m_StatusMessageAction;
        m_StatusMessageAction = StatusMessageAction::None;

        if (action == StatusMessageAction::ReloadLevel) {
            LoadLevel();
            StartLevelIntro(LEVEL_INTRO_DURATION);
            return;
        }
        if (action == StatusMessageAction::ShowGameOver) {
            BeginStatusMessage("GAME OVER", STATUS_MESSAGE_DURATION, StatusMessageAction::RestartToTitle);
            PlaySfx(m_Audio.gameOver.get());
            return;
        }
        if (action == StatusMessageAction::RestartToTitle) {
            Start();
            return;
        }
    }

    RenderStatusMessage();
}

void App::UpdateGameplay(float dt) {
    AdvanceAnimatedSprite(m_HudCoinIcon, dt);
    UpdateFloatingTexts(dt);
    const bool wasOnGround = m_Mario && m_Mario->IsOnGround();
    const bool wasFireMario = m_Mario && m_Mario->IsFire();
    const bool wasBigMario = m_Mario && m_Mario->IsBig();
    const bool powerupFreezeActive = m_Mario && m_Mario->IsTransforming();
    if (g_MapManager && !powerupFreezeActive) {
        g_MapManager->Update();
    }

    if (!powerupFreezeActive) {
        LootType lootType;
        glm::vec2 lootPos;
        while (g_MapManager && g_MapManager->PollSpawnEvent(lootType, lootPos)) {
            m_Pickups.push_back(std::make_unique<Pickup>(lootType, lootPos.x, lootPos.y));
            PlaySfx(m_Audio.bump.get());
            if (lootType == LootType::Coin) {
                PlaySfx(m_Audio.coin.get());
            } else {
                PlaySfx(m_Audio.powerUpAppears.get());
            }
        }

        glm::vec2 breakPos;
        while (g_MapManager && g_MapManager->PollBrickBreakEvent(breakPos)) {
            AwardPoints(50, breakPos + glm::vec2(0.0f, 52.0f));
            PlaySfx(m_Audio.breakBlock.get());
            m_Debris.push_back(std::make_unique<Debris>(breakPos.x, breakPos.y, Debris::Piece::TopLeft));
            m_Debris.push_back(std::make_unique<Debris>(breakPos.x, breakPos.y, Debris::Piece::TopRight));
            m_Debris.push_back(std::make_unique<Debris>(breakPos.x, breakPos.y, Debris::Piece::BottomLeft));
            m_Debris.push_back(std::make_unique<Debris>(breakPos.x, breakPos.y, Debris::Piece::BottomRight));

            const float tileSize = g_MapManager->GetTileSize();
            const float brickTop = breakPos.y + tileSize * 0.5f;
            for (auto& enemy : m_Enemies) {
                if (!enemy->IsAlive()) continue;

                const glm::vec2 enemyHalf = enemy->GetHalfExtents();
                const float enemyBottom = enemy->m_Transform.translation.y - enemyHalf.y;
                const bool overlapsBrickX =
                    std::abs(enemy->m_Transform.translation.x - breakPos.x) <= (tileSize * 0.5f + enemyHalf.x - 2.0f);
                const bool standingOnBrick =
                    enemyBottom >= brickTop - 8.0f &&
                    enemyBottom <= brickTop + tileSize * 0.75f;
                if (overlapsBrickX && standingOnBrick) {
                    const float horizontalKnockback =
                        (enemy->m_Transform.translation.x >= breakPos.x) ? 80.0f : -80.0f;
                    enemy->KillFlipped(horizontalKnockback);
                    AwardPoints(100, enemy->m_Transform.translation);
                    PlaySfx(m_Audio.kick.get());
                    PlaySfx(m_Audio.bowserFalls.get());
                }
            }
        }

        glm::vec2 coinPos;
        while (g_MapManager && g_MapManager->PollCoinCollectEvent(coinPos)) {
            ++m_Coins;
            AwardPoints(100, coinPos);
            PlaySfx(m_Audio.coin.get());
        }
    }

    const bool goalSequenceActive = m_GoalSequenceStage != GoalSequenceStage::None;
    const bool manualGoalControl = m_GoalSequenceStage == GoalSequenceStage::PlayerControl;
    const bool autoGoalSequence = goalSequenceActive && !manualGoalControl;
    if (autoGoalSequence) {
        UpdateGoalSequence(dt);
    } else {
        if (m_Mario) {
            const bool shouldPlayJumpSound =
                !powerupFreezeActive &&
                !goalSequenceActive &&
                wasOnGround &&
                !m_Mario->IsDead() &&
                Util::Input::IsKeyDown(Util::Keycode::SPACE);
            m_Mario->Update();
            if (shouldPlayJumpSound && !m_Mario->IsOnGround()) {
                PlaySfx(m_Mario->IsBig() ? m_Audio.jumpSuper.get() : m_Audio.jumpSmall.get());
            }
        }
        if (!powerupFreezeActive) {
            ActivateNearbyEnemies();
            for (auto& enemy : m_Enemies) {
                enemy->Update();
            }
            for (size_t i = 0; i < m_Enemies.size(); ++i) {
                Enemy* leftEnemy = m_Enemies[i].get();
                if (!leftEnemy->IsAlive()) continue;

                for (size_t j = i + 1; j < m_Enemies.size(); ++j) {
                    Enemy* rightEnemy = m_Enemies[j].get();
                    if (!rightEnemy->IsAlive()) continue;

                    const glm::vec2 leftHalf = leftEnemy->GetHalfExtents();
                    const glm::vec2 rightHalf = rightEnemy->GetHalfExtents();

                    const float leftLeft = leftEnemy->m_Transform.translation.x - leftHalf.x;
                    const float leftRight = leftEnemy->m_Transform.translation.x + leftHalf.x;
                    const float leftBottom = leftEnemy->m_Transform.translation.y - leftHalf.y;
                    const float leftTop = leftEnemy->m_Transform.translation.y + leftHalf.y;

                    const float rightLeft = rightEnemy->m_Transform.translation.x - rightHalf.x;
                    const float rightRight = rightEnemy->m_Transform.translation.x + rightHalf.x;
                    const float rightBottom = rightEnemy->m_Transform.translation.y - rightHalf.y;
                    const float rightTop = rightEnemy->m_Transform.translation.y + rightHalf.y;

                    const bool overlapX = leftRight > rightLeft && leftLeft < rightRight;
                    const bool overlapY = leftTop > rightBottom && leftBottom < rightTop;
                    if (!overlapX || !overlapY) continue;

                    const float overlapAmount =
                        std::min(leftRight, rightRight) - std::max(leftLeft, rightLeft);
                    if (overlapAmount <= 0.0f) continue;

                    const float separation = overlapAmount * 0.5f + 0.05f;
                    if (leftEnemy->m_Transform.translation.x <= rightEnemy->m_Transform.translation.x) {
                        leftEnemy->m_Transform.translation.x -= separation;
                        rightEnemy->m_Transform.translation.x += separation;
                    } else {
                        leftEnemy->m_Transform.translation.x += separation;
                        rightEnemy->m_Transform.translation.x -= separation;
                    }

                    leftEnemy->SetDirection(-leftEnemy->GetDirection());
                    rightEnemy->SetDirection(-rightEnemy->GetDirection());
                }
            }
            for (auto& fireball : m_Fireballs) {
                fireball->Update();
            }
            for (auto& pickup : m_Pickups) {
                pickup->Update();
                if (pickup->GetType() == LootType::Coin && pickup->ConsumeAutoAward()) {
                    ++m_Coins;
                    AwardPoints(200, pickup->m_Transform.translation + glm::vec2(0.0f, 42.0f));
                    PlaySfx(m_Audio.coin.get());
                }
            }
            for (auto& debris : m_Debris) {
                debris->Update();
            }
        }
    }

    if (!autoGoalSequence && !powerupFreezeActive && m_Mario && !m_Mario->IsDead()) {
        m_LevelTimer = std::max(0.0f, m_LevelTimer - dt);
        if (m_LevelTimer <= 0.0f) {
            m_DeathWasTimeout = true;
            m_Mario->Die();
        }

        const int displayedLevelTime = DisplayLevelTime(m_LevelTimer);
        if (displayedLevelTime != m_DisplayedLevelTime) {
            m_DisplayedLevelTime = displayedLevelTime;
            SetSpriteText(m_HudTimeValue, PadNumber(m_DisplayedLevelTime, 3));
        }
    }

    if (!m_WasMarioDead && m_Mario && m_Mario->IsDead()) {
        m_Lives = std::max(0, m_Lives - 1);
        RefreshHudText();
        StopMusic(200);
        PlaySfx(m_Audio.marioDie.get());
    }
    m_WasMarioDead = m_Mario && m_Mario->IsDead();

    if (m_Mario && !m_Mario->IsDead()) {
        const bool marioPoweredDown =
            (wasFireMario && !m_Mario->IsFire()) ||
            (!wasFireMario && wasBigMario && !m_Mario->IsBig());
        if (marioPoweredDown) {
            PlaySfx(m_Audio.warning.get());
        }
    }

    if (m_Mario && m_Mario->IsDeathSequenceFinished()) {
        HandleMarioDeath();
        return;
    }

    if (!powerupFreezeActive) {
        m_FireballCooldown = std::max(0.0f, m_FireballCooldown - dt);
    }
    if (!autoGoalSequence && !powerupFreezeActive &&
        m_Mario && m_Mario->IsFire() && !m_Mario->IsDead() &&
        Util::Input::IsKeyDown(Util::Keycode::F) &&
        m_FireballCooldown <= 0.0f &&
        m_Fireballs.size() < 2) {
        const glm::vec2 spawn = m_Mario->GetFireballSpawnPosition();
        m_Fireballs.push_back(std::make_unique<Fireball>(
            spawn.x,
            spawn.y,
            m_Mario->GetFacingDirection()
        ));
        m_FireballCooldown = 0.18f;
        PlaySfx(m_Audio.fireball.get());
        PlaySfx(m_Audio.bowserFire.get());
    }

    if (!m_LowTimeWarningPlayed && !autoGoalSequence && m_Mario && !m_Mario->IsDead() &&
        m_DisplayedLevelTime > 0 && m_DisplayedLevelTime <= 100) {
        m_LowTimeWarningPlayed = true;
        PlaySfx(m_Audio.warning.get());
    }

    if (!autoGoalSequence && !powerupFreezeActive && m_Mario && !m_Mario->IsDead()) {
        const glm::vec2 marioHalf = m_Mario->GetHalfExtents();
        for (auto& enemy : m_Enemies) {
            if (!enemy->IsAlive()) continue;

            const glm::vec2 enemyHalf = enemy->GetHalfExtents();
            const float marioLeft = m_Mario->m_Transform.translation.x - marioHalf.x;
            const float marioRight = m_Mario->m_Transform.translation.x + marioHalf.x;
            const float marioBottom = m_Mario->m_Transform.translation.y - marioHalf.y;
            const float marioTop = m_Mario->m_Transform.translation.y + marioHalf.y;

            const float enemyLeft = enemy->m_Transform.translation.x - enemyHalf.x;
            const float enemyRight = enemy->m_Transform.translation.x + enemyHalf.x;
            const float enemyBottom = enemy->m_Transform.translation.y - enemyHalf.y;
            const float enemyTop = enemy->m_Transform.translation.y + enemyHalf.y;

            const bool overlapX = marioRight > enemyLeft && marioLeft < enemyRight;
            const bool overlapY = marioTop > enemyBottom && marioBottom < enemyTop;
            if (!overlapX || !overlapY) continue;

            const float horizontalOverlap =
                std::min(marioRight, enemyRight) - std::max(marioLeft, enemyLeft);
            const float verticalOverlap =
                std::min(marioTop, enemyTop) - std::max(marioBottom, enemyBottom);
            const float stompForgiveness = 18.0f;
            const float minimumStompWidth = enemyHalf.x * 0.35f;
            const bool stomped =
                m_Mario->GetVelocityY() < -30.0f &&
                m_Mario->m_Transform.translation.y >= enemy->m_Transform.translation.y - 8.0f &&
                marioBottom >= enemyTop - stompForgiveness &&
                horizontalOverlap >= minimumStompWidth &&
                verticalOverlap <= enemyHalf.y + stompForgiveness;

            if (m_Mario->HasStarPower()) {
                enemy->KillFlipped(110.0f * m_Mario->GetFacingDirection());
                AwardPoints(100, enemy->m_Transform.translation);
                PlaySfx(m_Audio.kick.get());
                PlaySfx(m_Audio.bowserFalls.get());
            } else if (stomped) {
                enemy->Stomp();
                m_Mario->BounceAfterStomp();
                AwardPoints(100, enemy->m_Transform.translation);
                PlaySfx(m_Audio.stomp.get());
            } else if (!m_Mario->IsInvulnerable()) {
                m_Mario->TakeEnemyHit();
                break;
            }
        }

        for (auto& pickup : m_Pickups) {
            if (pickup->IsCollected()) continue;
            const glm::vec2 pickupHalf = pickup->GetHalfExtents();
            const float pickupLeft = pickup->m_Transform.translation.x - pickupHalf.x;
            const float pickupRight = pickup->m_Transform.translation.x + pickupHalf.x;
            const float pickupBottom = pickup->m_Transform.translation.y - pickupHalf.y;
            const float pickupTop = pickup->m_Transform.translation.y + pickupHalf.y;
            const float marioLeft = m_Mario->m_Transform.translation.x - marioHalf.x;
            const float marioRight = m_Mario->m_Transform.translation.x + marioHalf.x;
            const float marioBottom = m_Mario->m_Transform.translation.y - marioHalf.y;
            const float marioTop = m_Mario->m_Transform.translation.y + marioHalf.y;
            const bool overlapX = marioRight > pickupLeft && marioLeft < pickupRight;
            const bool overlapY = marioTop > pickupBottom && marioBottom < pickupTop;
            if (overlapX && overlapY) {
                const LootType type = pickup->GetType();
                const glm::vec2 rewardPos = pickup->m_Transform.translation;
                m_Mario->PowerUp(type);
                if (type == LootType::Coin) {
                    ++m_Coins;
                    AwardPoints(100, rewardPos);
                    PlaySfx(m_Audio.coin.get());
                } else if (type == LootType::GreenMushroom) {
                    ++m_Lives;
                    AwardPoints(1000, rewardPos, "1UP");
                    PlaySfx(m_Audio.oneUp.get());
                } else {
                    AwardPoints(1000, rewardPos);
                    PlaySfx(m_Audio.powerUp.get());
                }
                pickup->Collect();
                RefreshHudText();
            }
        }

        for (auto& fireball : m_Fireballs) {
            if (fireball->IsExpired() || fireball->IsExploding()) continue;

            const glm::vec2 fireballHalf = fireball->GetHalfExtents();
            const float fireballLeft = fireball->m_Transform.translation.x - fireballHalf.x;
            const float fireballRight = fireball->m_Transform.translation.x + fireballHalf.x;
            const float fireballBottom = fireball->m_Transform.translation.y - fireballHalf.y;
            const float fireballTop = fireball->m_Transform.translation.y + fireballHalf.y;

            for (auto& enemy : m_Enemies) {
                if (!enemy->IsAlive()) continue;

                const glm::vec2 enemyHalf = enemy->GetHalfExtents();
                const float enemyLeft = enemy->m_Transform.translation.x - enemyHalf.x;
                const float enemyRight = enemy->m_Transform.translation.x + enemyHalf.x;
                const float enemyBottom = enemy->m_Transform.translation.y - enemyHalf.y;
                const float enemyTop = enemy->m_Transform.translation.y + enemyHalf.y;

                const bool overlapX = fireballRight > enemyLeft && fireballLeft < enemyRight;
                const bool overlapY = fireballTop > enemyBottom && fireballBottom < enemyTop;
                if (!overlapX || !overlapY) continue;

                enemy->Stomp();
                fireball->Explode();
                AwardPoints(100, enemy->m_Transform.translation);
                PlaySfx(m_Audio.kick.get());
                PlaySfx(m_Audio.bowserFalls.get());
                break;
            }
        }

        if (g_MapManager && g_MapManager->HasGoal() &&
            m_GoalSequenceStage == GoalSequenceStage::None &&
            m_Mario->m_Transform.translation.x >= g_MapManager->GetGoalX()) {
            StartGoalSequence();
        }
    }

    if (m_GoalSequenceStage == GoalSequenceStage::Entering) {
        UpdateGoalSequence(dt);
    }
    if (m_GoalSequenceStage == GoalSequenceStage::PlayerControl) {
        UpdateGoalSequence(dt);
    }

    m_Enemies.erase(
        std::remove_if(m_Enemies.begin(), m_Enemies.end(),
                       [](const std::unique_ptr<Enemy>& enemy) { return enemy->IsDeadAndExpired(); }),
        m_Enemies.end()
    );
    m_Fireballs.erase(
        std::remove_if(m_Fireballs.begin(), m_Fireballs.end(),
                       [](const std::unique_ptr<Fireball>& fireball) { return fireball->IsExpired(); }),
        m_Fireballs.end()
    );
    m_Pickups.erase(
        std::remove_if(m_Pickups.begin(), m_Pickups.end(),
                       [](const std::unique_ptr<Pickup>& pickup) { return pickup->IsCollected(); }),
        m_Pickups.end()
    );
    m_Debris.erase(
        std::remove_if(m_Debris.begin(), m_Debris.end(),
                       [](const std::unique_ptr<Debris>& debris) { return debris->IsExpired(); }),
        m_Debris.end()
    );

    if (m_Mario && g_MapManager) {
        m_ViewX = m_Mario->m_Transform.translation.x;

        float mapLeft = g_MapManager->GetWorldLeft();
        float mapRight = g_MapManager->GetWorldRight();
        float halfScreen = WINDOW_WIDTH / 2.0f;
        float minViewX = mapLeft + halfScreen;
        float maxViewX = mapRight - halfScreen;

        if (minViewX > maxViewX) {
            m_ViewX = 0.0f;
        } else {
            m_ViewX = std::clamp(m_ViewX, minViewX, maxViewX);
        }
    }

    RenderGameplay();
}

void App::Update() {
    const float dt = std::max(0.001f, Util::Time::GetDeltaTimeMs() / 1000.0f);

    if ((m_ScreenState == ScreenState::Gameplay || m_ScreenState == ScreenState::Paused) &&
        (Util::Input::IsKeyDown(Util::Keycode::P) || Util::Input::IsKeyDown(Util::Keycode::PAUSE))) {
        TogglePause();
    }

    switch (m_ScreenState) {
    case ScreenState::Title:
        UpdateTitleScreen(dt);
        break;
    case ScreenState::LevelIntro:
        UpdateLevelIntro(dt);
        break;
    case ScreenState::Gameplay:
        UpdateGameplay(dt);
        break;
    case ScreenState::Paused:
        UpdatePaused(dt);
        break;
    case ScreenState::StatusMessage:
        UpdateStatusMessage(dt);
        break;
    }

    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) || Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() {
    LOG_TRACE("End");
}
