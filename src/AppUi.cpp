#include "App.hpp"
#include "AppDetail.hpp"
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

using namespace AppDetail;

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
    m_IntroMario->m_Transform.translation = { -44.0f, -18.0f };
    m_IntroMario->m_Transform.scale = { 3.0f, 3.0f };
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

    ConfigureSpriteText(m_IntroWorldText, { -90.0f, 68.0f }, { INTRO_TEXT_SCALE, INTRO_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
    ConfigureSpriteText(m_IntroLivesText, { 20.0f, -18.0f }, { INTRO_TEXT_SCALE, INTRO_TEXT_SCALE }, 3.0f, 0.0f, UI_Z);
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
    m_LevelIntroPurpose = LevelIntroPurpose::StartLevel;
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

