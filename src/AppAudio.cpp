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

void App::InitializeAudio() {
    if (m_Audio.groundTheme != nullptr) return;

    m_Audio.groundTheme = std::make_unique<Util::BGM>(AssetPaths::Sound("01. Ground Theme.mp3"));
    m_Audio.invincibilityTheme = std::make_unique<Util::BGM>(AssetPaths::Sound("05. Invincibility Theme.mp3"));
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
    if (m_Audio.invincibilityTheme != nullptr) {
        m_Audio.invincibilityTheme->SetVolume(48);
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

void App::PlayInvincibilityMusic(bool restart) {
    InitializeAudio();
    if (m_Audio.invincibilityTheme == nullptr) return;
    if (!restart && m_ActiveMusic == MusicTrack::InvincibilityTheme) return;
    m_Audio.invincibilityTheme->Play(-1);
    m_ActiveMusic = MusicTrack::InvincibilityTheme;
}

void App::UpdateGameplayMusic() {
    if (!m_Mario || m_Mario->IsDead() || m_GoalSequenceStage != GoalSequenceStage::None) {
        return;
    }

    const bool useInvincibilityTheme =
        m_Mario->HasStarPower() || m_Mario->IsRecoveringFromHit();
    if (useInvincibilityTheme) {
        PlayInvincibilityMusic();
    } else {
        PlayGameplayMusic();
    }
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

