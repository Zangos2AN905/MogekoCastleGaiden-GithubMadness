#include <SDL.h>

#ifndef NO_AUDIO
#include <SDL_mixer_ext.h>
#endif

#include "ui/loading_screen.hpp"
#include "ui/dialogue_system.hpp"
#include "engine/rpg_rt_transition.hpp"
#include "engine/title.hpp"
#include "game/cutscenes/warning.hpp"

#include <functional>
#include <iostream>
#include <string>

// Window dimensions
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;
const char* WINDOW_TITLE = "Mogeko Castle Gaiden: Github Madness";

// Game state
enum class GameState {
    Loading,
    Warning,
    Intro,
    Title,
    Playing
};

bool g_running = true;
GameState g_state = GameState::Loading;
SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
Uint32 g_last_time = 0;
Uint32 g_transition_timer = 0;

// UI Systems
LoadingScreen g_loading_screen;
DialogueSystem g_dialogue_system;
TitleScreen g_title_screen;
WarningScreen g_warning_screen;
RpgRtTransition g_transition;

#ifndef NO_AUDIO
Mix_Music* g_music = nullptr;
Mix_Music* g_introMusic = nullptr;
Mix_Music* g_titleMusic = nullptr;
#endif

void playMusic(Mix_Music* music, int loops = -1);

void renderState(GameState state) {
    switch (state) {
        case GameState::Loading:
            g_loading_screen.render(g_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
            break;
        case GameState::Warning:
            g_warning_screen.render(g_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
            break;
        case GameState::Intro:
            SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
            SDL_RenderClear(g_renderer);
            g_dialogue_system.render(g_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
            break;
        case GameState::Title:
            g_title_screen.render(g_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
            break;
        case GameState::Playing:
            SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
            SDL_RenderClear(g_renderer);
            break;
    }
}

void enterState(GameState newState) {
    g_state = newState;

    switch (g_state) {
        case GameState::Warning:
            g_warning_screen.reset();
            break;
        case GameState::Intro:
            g_dialogue_system.start();
#ifndef NO_AUDIO
            playMusic(g_introMusic);
#endif
            break;
        case GameState::Title:
#ifndef NO_AUDIO
            playMusic(g_titleMusic);
#endif
            break;
        case GameState::Playing:
            g_title_screen.reset();
            std::cout << "Starting game..." << std::endl;
            break;
        default:
            break;
    }
}

void requestStateChange(GameState newState,
                        RpgRtTransition::Type transitionType = RpgRtTransition::Type::FadeIn) {
    if (g_renderer == nullptr || g_transition.isActive()) {
        enterState(newState);
        return;
    }

    renderState(g_state);
    SDL_Surface* fromSurface = RpgRtTransition::CaptureRenderer(g_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    enterState(newState);
    renderState(g_state);
    SDL_Surface* toSurface = RpgRtTransition::CaptureRenderer(g_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    if (fromSurface != nullptr && toSurface != nullptr) {
        if (g_transition.begin(g_renderer, transitionType, fromSurface, toSurface)) {
            g_transition_timer = 0;
        }
    }

    if (fromSurface != nullptr) {
        SDL_FreeSurface(fromSurface);
    }
    if (toSurface != nullptr) {
        SDL_FreeSurface(toSurface);
    }
}

void updateTransition(Uint32 delta_time) {
    if (!g_transition.isActive()) {
        g_transition_timer = 0;
        return;
    }

    g_transition_timer += delta_time;
    constexpr Uint32 kFrameLengthMs = 1000 / 60;
    const int framesToAdvance = static_cast<int>(g_transition_timer / kFrameLengthMs);
    if (framesToAdvance <= 0) {
        return;
    }

    g_transition.advance(framesToAdvance);
    g_transition_timer -= static_cast<Uint32>(framesToAdvance) * kFrameLengthMs;

    if (g_transition.isFinished()) {
        g_transition.reset();
        g_transition_timer = 0;
    }
}

// Helper to play music
void playMusic(Mix_Music* music, int loops) {
#ifndef NO_AUDIO
    if (music) {
        Mix_HaltMusic();
        Mix_PlayMusic(music, loops);
    }
#else
    (void)music;
    (void)loops;
#endif
}

bool init() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create window
    g_window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (g_window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create renderer
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_renderer == nullptr) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

#ifndef NO_AUDIO
    // Initialize SDL_mixer_ext - try to init formats but don't fail if some are missing
    int mixFlags = MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_FLAC;
    int mixInitted = Mix_Init(mixFlags);
    if (mixInitted == 0) {
        std::cerr << "SDL_mixer_ext: No audio format support available." << std::endl;
    } else {
        // Report which formats are available
        std::cout << "Audio formats available:";
        if (mixInitted & MIX_INIT_MP3) std::cout << " MP3";
        if (mixInitted & MIX_INIT_OGG) std::cout << " OGG";
        if (mixInitted & MIX_INIT_FLAC) std::cout << " FLAC";
        std::cout << std::endl;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL_mixer_ext could not open audio! Mix_Error: " << Mix_GetError() << std::endl;
        // Continue without audio
    }
#endif

    // Initialize loading screen
    if (!g_loading_screen.init(g_renderer)) {
        std::cerr << "Failed to initialize loading screen!" << std::endl;
        // Continue anyway, we can render without it
    }

    // Initialize dialogue system
    if (!g_dialogue_system.init(g_renderer, "assets/fonts/determination.ttf", 28)) {
        std::cerr << "Failed to initialize dialogue system!" << std::endl;
    }

    if (!g_warning_screen.init("assets/fonts/determination.ttf", 28)) {
        std::cerr << "Failed to initialize warning screen!" << std::endl;
    }

    // Load intro dialogue
    if (!g_dialogue_system.loadDialogue("assets/dialogue/begin.jsonc")) {
        std::cerr << "Failed to load intro dialogue!" << std::endl;
    }

    // Set callback for when intro dialogue completes
    g_dialogue_system.setOnComplete([]() {
        std::cout << "Intro complete, moving to title..." << std::endl;
        requestStateChange(GameState::Title);
    });

    // Initialize title screen
    if (!g_title_screen.init(g_renderer)) {
        std::cerr << "Failed to initialize title screen!" << std::endl;
    }

#ifndef NO_AUDIO
    // Load music
    g_introMusic = Mix_LoadMUS("assets/music/storytime.mp3");
    if (!g_introMusic) {
        std::cerr << "Failed to load intro music: " << Mix_GetError() << std::endl;
    }
    
    g_titleMusic = Mix_LoadMUS("assets/music/Theme3.mid");
    if (!g_titleMusic) {
        std::cerr << "Failed to load title music: " << Mix_GetError() << std::endl;
    }
#endif

    g_last_time = SDL_GetTicks();

    return true;
}

void cleanup() {
    g_loading_screen.cleanup();
    g_title_screen.cleanup();
    g_warning_screen.cleanup();
    g_dialogue_system.cleanup();

#ifndef NO_AUDIO
    if (g_music != nullptr) {
        Mix_FreeMusic(g_music);
        g_music = nullptr;
    }
    if (g_introMusic != nullptr) {
        Mix_FreeMusic(g_introMusic);
        g_introMusic = nullptr;
    }
    if (g_titleMusic != nullptr) {
        Mix_FreeMusic(g_titleMusic);
        g_titleMusic = nullptr;
    }
    Mix_CloseAudio();
    Mix_Quit();
#endif

    if (g_renderer != nullptr) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = nullptr;
    }

    if (g_window != nullptr) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
    }

    SDL_Quit();
}

void handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                g_running = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    g_running = false;
                }
                if (g_transition.isActive()) {
                    break;
                }
                // Handle input based on state
                switch (g_state) {
                    case GameState::Loading:
                        // Skip loading screen on any key press
                        g_loading_screen.setComplete(true);
                        break;
                    case GameState::Warning:
                        g_warning_screen.handleKeyDown(event.key.keysym.sym);
                        break;
                    case GameState::Intro:
                        // Advance dialogue
                        if (event.key.keysym.sym == SDLK_RETURN || 
                            event.key.keysym.sym == SDLK_SPACE ||
                            event.key.keysym.sym == SDLK_z) {
                            g_dialogue_system.advance();
                        }
                        break;
                    case GameState::Title:
                        g_title_screen.handleKeyDown(event.key.keysym.sym);
                        break;
                    default:
                        break;
                }
                break;
        }
    }
}

void update(Uint32 delta_time) {
    updateTransition(delta_time);
    if (g_transition.isActive()) {
        return;
    }

    switch (g_state) {
        case GameState::Loading:
            g_loading_screen.update(delta_time);
            // Check if loading is complete
            if (g_loading_screen.isComplete()) {
                std::cout << "Loading complete, showing warning..." << std::endl;
                requestStateChange(GameState::Warning);
            }
            break;
        case GameState::Warning:
            g_warning_screen.update(delta_time);
            if (g_warning_screen.isComplete()) {
                std::cout << "Warning acknowledged, starting intro..." << std::endl;
                requestStateChange(GameState::Intro);
            }
            break;
        case GameState::Intro:
            g_dialogue_system.update(delta_time);
            break;
        case GameState::Title:
            g_title_screen.update(delta_time);
            if (g_title_screen.shouldStartGame()) {
                requestStateChange(GameState::Playing);
            }
            break;
        case GameState::Playing:
            // Game update
            break;
    }
}

void render() {
    if (g_transition.isActive()) {
        g_transition.render(g_renderer);
    } else {
        renderState(g_state);
    }

    // Present
    SDL_RenderPresent(g_renderer);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "==================================" << std::endl;
    std::cout << " Mogeko Castle Gaiden: Github Madness" << std::endl;
    std::cout << "==================================" << std::endl;

    if (!init()) {
        std::cerr << "Failed to initialize!" << std::endl;
        cleanup();
        return 1;
    }

    std::cout << "Press ESC or close the window to exit." << std::endl;

    // Main game loop
    while (g_running) {
        Uint32 current_time = SDL_GetTicks();
        Uint32 delta_time = current_time - g_last_time;
        g_last_time = current_time;

        handleEvents();
        update(delta_time);
        render();
    }

    cleanup();
    std::cout << "Goodbye!" << std::endl;

    return 0;
}
