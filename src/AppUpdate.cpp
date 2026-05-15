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

void App::UpdateTitleScreen(float dt) {
    AdvanceAnimatedSprite(m_TitleCoinIcon, dt);
    m_TitleBlinkTimer += dt;

    if (Util::Input::IsKeyDown(Util::Keycode::UP) ||
        Util::Input::IsKeyDown(Util::Keycode::DOWN) ||
        Util::Input::IsKeyDown(Util::Keycode::W) ||
        Util::Input::IsKeyDown(Util::Keycode::S)) {
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
        if (m_LevelIntroPurpose == LevelIntroPurpose::PostGoalTransition) {
            LoadTransitionScene();
            return;
        }
        PlayGameplayMusic(true);
        m_ScreenState = ScreenState::Gameplay;
    }

    RenderLevelIntro();
}

void App::UpdatePaused(float) {
    RenderPaused();
}

void App::UpdateTransitionScene(float dt) {
    AdvanceAnimatedSprite(m_HudCoinIcon, dt);

    if (g_MapManager) {
        g_MapManager->Update();
    }

    if (m_Mario) {
        const bool wasOnGround = m_Mario->IsOnGround();
        if (!m_TransitionPipeReached) {
            m_Mario->Update();
            if (g_MapManager) {
                const float entryRange =
                    g_MapManager->GetTileSize() * TRANSITION_PIPE_ENTRY_RANGE_TILES;
                const bool atPipe =
                    std::abs(m_Mario->m_Transform.translation.x - m_TransitionPipeEntryX) <= entryRange;
                const bool pressDown =
                    Util::Input::IsKeyPressed(Util::Keycode::S) ||
                    Util::Input::IsKeyPressed(Util::Keycode::DOWN);
                if (atPipe && wasOnGround && m_Mario->IsOnGround() && pressDown) {
                    m_Mario->m_Transform.translation.x = m_TransitionPipeEntryX;
                    m_Mario->m_Transform.translation.y = m_TransitionPipeEntryY;
                    m_TransitionPipeReached = true;
                    m_TransitionPipeEntryY = m_Mario->m_Transform.translation.y;
                    m_Mario->SetVisible(true);
                }
            }
        } else if (!m_TransitionMarioHidden) {
            if (!m_TransitionPipeSoundPlayed) {
                m_TransitionPipeSoundPlayed = true;
                PlaySfx(m_Audio.pipe.get());
            }
            m_Mario->m_Transform.translation.x = m_TransitionPipeEntryX;
            m_Mario->m_Transform.translation.y -= TRANSITION_PIPE_SINK_SPEED * dt;
            if (m_Mario->m_Transform.translation.y <= m_TransitionPipeEntryY - m_TransitionPipeSinkDistance) {
                m_Mario->SetVisible(false);
                m_TransitionMarioHidden = true;
            }
        } else {
            m_TransitionExitTimer = std::max(0.0f, m_TransitionExitTimer - dt);
            if (m_TransitionExitTimer <= 0.0f) {
                m_CurrentState = State::END;
                return;
            }
        }
    }

    if (m_Mario && g_MapManager) {
        const float halfScreen = WINDOW_WIDTH / 2.0f;
        const float minViewX = g_MapManager->GetWorldLeft() + halfScreen;
        const float maxViewX = g_MapManager->GetWorldRight() - halfScreen;
        if (minViewX > maxViewX) {
            m_ViewX = 0.0f;
        } else {
            m_ViewX = std::clamp(m_Mario->m_Transform.translation.x, minViewX, maxViewX);
        }
    }

    RenderSceneWorld(false);
    DrawHud();
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
    const bool autoGoalSequence = goalSequenceActive;
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

                    if (leftEnemy->IsShellSliding() && rightEnemy->CanBeDefeatedByShell()) {
                        if (rightEnemy->IsShellSliding()) {
                            leftEnemy->SetDirection(-leftEnemy->GetDirection());
                            rightEnemy->SetDirection(-rightEnemy->GetDirection());
                        } else {
                            const float knockback = 180.0f * leftEnemy->GetDirection();
                            rightEnemy->KillFlipped(knockback);
                            AwardPoints(100, rightEnemy->m_Transform.translation);
                            PlaySfx(m_Audio.kick.get());
                            PlaySfx(m_Audio.bowserFalls.get());
                        }
                        continue;
                    }
                    if (rightEnemy->IsShellSliding() && leftEnemy->CanBeDefeatedByShell()) {
                        const float knockback = 180.0f * rightEnemy->GetDirection();
                        leftEnemy->KillFlipped(knockback);
                        AwardPoints(100, leftEnemy->m_Transform.translation);
                        PlaySfx(m_Audio.kick.get());
                        PlaySfx(m_Audio.bowserFalls.get());
                        continue;
                    }

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

    UpdateGameplayMusic();

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
            const bool kickableShell = enemy->IsKickableShell();
            const bool sideShellContact = kickableShell && !stomped;
            const float shellKickDirection =
                (m_Mario->m_Transform.translation.x >= enemy->m_Transform.translation.x) ? -1.0f : 1.0f;

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
            } else if (sideShellContact) {
                enemy->KickShell(shellKickDirection);
                PlaySfx(m_Audio.kick.get());
            } else if (enemy->IsHarmlessToPlayer()) {
                continue;
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

                enemy->KillFlipped(140.0f * fireball->GetDirection());
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
    case ScreenState::TransitionScene:
        UpdateTransitionScene(dt);
        break;
    }

    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) || Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

