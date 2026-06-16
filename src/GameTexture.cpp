#include "GameTexture.hpp"

#include "Core/TextureUtils.hpp"
#include "Util/Logger.hpp"

GameTexture::GameTexture(GLint format, int width, int height, const void* data) {
    glGenTextures(1, &m_TextureId);
    UpdateData(format, width, height, data);
}

GameTexture::GameTexture(GameTexture&& texture) noexcept {
    m_TextureId = texture.m_TextureId;
    texture.m_TextureId = 0;
}

GameTexture::~GameTexture() {
    glDeleteTextures(1, &m_TextureId);
}

GameTexture& GameTexture::operator=(GameTexture&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    glDeleteTextures(1, &m_TextureId);
    m_TextureId = other.m_TextureId;
    other.m_TextureId = 0;
    return *this;
}

void GameTexture::Bind(int slot) const {
    int maxCount = 0;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxCount);

    if (slot >= maxCount) {
        LOG_ERROR("Maximum texture count exceeded");
        return;
    }

    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_TextureId);
}

void GameTexture::Unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void GameTexture::UpdateData(GLint format, int width, int height,
                             const void* data) {
    glBindTexture(GL_TEXTURE_2D, m_TextureId);

    glTexImage2D(GL_TEXTURE_2D, 0, Core::GlFormatToGlInternalFormat(format),
                 width, height, 0, format, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
