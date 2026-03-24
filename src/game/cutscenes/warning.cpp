#include "warning.hpp"

#include <cmath>
#include <iostream>

namespace {
constexpr Uint32 kInputDelayMs = 1200;
constexpr const char* kWarningVoicePath = "assets/sounds/warning.mp3";
const SDL_Color kTitleColor = {255, 96, 96, 255};
const SDL_Color kBodyColor = {240, 240, 240, 255};
}

WarningScreen::WarningScreen() = default;

WarningScreen::~WarningScreen() {
    cleanup();
}

bool WarningScreen::init(const std::string& fontPath, int fontSize) {
    if (TTF_WasInit() == 0 && TTF_Init() == -1) {
        std::cerr << "SDL_ttf could not initialize for warning screen! TTF_Error: "
                  << TTF_GetError() << std::endl;
        return false;
    }

    m_font = TTF_OpenFont(fontPath.c_str(), fontSize);
    if (!m_font) {
        std::cerr << "Failed to load warning font '" << fontPath << "': "
                  << TTF_GetError() << std::endl;
        return false;
    }

#ifndef NO_AUDIO
    m_voiceClip = Mix_LoadMUS(kWarningVoicePath);
    if (!m_voiceClip) {
        std::cerr << "Failed to load warning voice clip '" << kWarningVoicePath << "': "
                  << Mix_GetError() << std::endl;
    }
#endif

    reset();
    return true;
}

void WarningScreen::cleanup() {
    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }

#ifndef NO_AUDIO
    if (m_voiceClip) {
        Mix_FreeMusic(m_voiceClip);
        m_voiceClip = nullptr;
    }
#endif
}

void WarningScreen::reset() {
    m_elapsedTime = 0;
    m_inputEnabled = false;
    m_voiceStarted = false;
    m_complete = false;
}

void WarningScreen::update(Uint32 delta_time) {
    if (m_complete) {
        return;
    }

#ifndef NO_AUDIO
    if (!m_voiceStarted && m_voiceClip) {
        Mix_PlayMusic(m_voiceClip, 0);
        m_voiceStarted = true;
    }
#endif

    m_elapsedTime += delta_time;
    if (m_elapsedTime >= kInputDelayMs) {
        m_inputEnabled = true;
    }
}

void WarningScreen::handleKeyDown(SDL_Keycode key) {
    if (!m_inputEnabled) {
        return;
    }

    switch (key) {
        case SDLK_RETURN:
        case SDLK_SPACE:
        case SDLK_z:
        case SDLK_x:
#ifndef NO_AUDIO
            Mix_HaltMusic();
#endif
            m_complete = true;
            break;
        default:
            break;
    }
}

void WarningScreen::renderCenteredText(SDL_Renderer* renderer,
                                       const std::string& text,
                                       int centerX,
                                       int y,
                                       SDL_Color color,
                                       Uint32 wrapWidth) const {
    if (!m_font || text.empty()) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(m_font, text.c_str(), color, wrapWidth);
    if (!surface) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dest = {
            centerX - surface->w / 2,
            y,
            surface->w,
            surface->h
        };
        SDL_RenderCopy(renderer, texture, nullptr, &dest);
        SDL_DestroyTexture(texture);
    }

    SDL_FreeSurface(surface);
}

void WarningScreen::render(SDL_Renderer* renderer, int screen_width, int screen_height) {
    SDL_SetRenderDrawColor(renderer, 8, 0, 0, 255);
    SDL_RenderClear(renderer);

    const Uint8 accent = static_cast<Uint8>(120 + 40 * std::sin(SDL_GetTicks() / 300.0));
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, accent, 16, 16, 255);

    SDL_Rect topBar = {0, 0, screen_width, 10};
    SDL_Rect bottomBar = {0, screen_height - 10, screen_width, 10};
    SDL_RenderFillRect(renderer, &topBar);
    SDL_RenderFillRect(renderer, &bottomBar);

    SDL_Rect panel = {
        48,
        72,
        screen_width - 96,
        screen_height - 144
    };
    SDL_SetRenderDrawColor(renderer, 24, 0, 0, 220);
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 180, 48, 48, 255);
    SDL_RenderDrawRect(renderer, &panel);

    const int centerX = screen_width / 2;
    renderCenteredText(renderer, "WARNING", centerX, 108, kTitleColor, panel.w - 48);
    renderCenteredText(renderer,
                       "This game is horrid.",
                       centerX,
                       180,
                       kBodyColor,
                       panel.w - 64);
    renderCenteredText(renderer,
                       "Do not play if you want the original game and not a copy!",
                       centerX,
                       228,
                       kBodyColor,
                       panel.w - 64);

    SDL_Color promptColor = kBodyColor;
    promptColor.a = m_inputEnabled
        ? static_cast<Uint8>(160 + 95 * std::sin(SDL_GetTicks() / 180.0))
        : 96;
    renderCenteredText(renderer,
                       m_inputEnabled ? "PRESS Z, ENTER, OR SPACE" : "...",
                       centerX,
                       screen_height - 104,
                       promptColor,
                       panel.w - 64);
}