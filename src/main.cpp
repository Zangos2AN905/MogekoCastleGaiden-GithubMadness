#include <SDL.h>

#ifndef NO_AUDIO
#include <SDL_mixer_ext.h>
#endif

#include "ui/loading_screen.hpp"
#include "ui/dialogue_system.hpp"
#include "engine/title.hpp"

#include <iostream>
#include <string>

// Window dimensions
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;
const char* WINDOW_TITLE = "Mogeko Castle Gaiden: Github Madness";

// Game state
enum class GameState {
    Loading,
    Intro,
    Title,
    Playing
};

bool g_running = true;
GameState g_state = GameState::Loading;
SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
Uint32 g_last_time = 0;

// UI Systems
LoadingScreen g_loading_screen;
DialogueSystem g_dialogue_system;
TitleScreen g_title_screen;

#ifndef NO_AUDIO
Mix_Music* g_music = nullptr;
Mix_Music* g_introMusic = nullptr;
Mix_Music* g_titleMusic = nullptr;
#endif

// Helper to play music
void playMusic(Mix_Music* music, int loops = -1) {
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

    // Load intro dialogue
    if (!g_dialogue_system.loadDialogue("assets/dialogue/begin.jsonc")) {
        std::cerr << "Failed to load intro dialogue!" << std::endl;
    }

    // Set callback for when intro dialogue completes
    g_dialogue_system.setOnComplete([]() {
        std::cout << "Intro complete, moving to title..." << std::endl;
        g_state = GameState::Title;
#ifndef NO_AUDIO
        playMusic(g_titleMusic);
#endif
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
    g_dialogue_system.cleanup();
    g_title_screen.cleanup();

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
                // Handle input based on state
                switch (g_state) {
                    case GameState::Loading:
                        // Skip loading screen on any key press
                        g_loading_screen.setComplete(true);
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
    switch (g_state) {
        case GameState::Loading:
            g_loading_screen.update(delta_time);
            // Check if loading is complete
            if (g_loading_screen.isComplete()) {
                std::cout << "Loading complete, starting intro..." << std::endl;
                g_state = GameState::Intro;
                g_dialogue_system.start();
#ifndef NO_AUDIO
                playMusic(g_introMusic);
#endif
            }
            break;
        case GameState::Intro:
            g_dialogue_system.update(delta_time);
            break;
        case GameState::Title:
            g_title_screen.update(delta_time);
            if (g_title_screen.shouldStartGame()) {
                g_title_screen.reset();
                g_state = GameState::Playing;
                std::cout << "Starting game..." << std::endl;
            }
            break;
        case GameState::Playing:
            // Game update
            break;
    }
}

void render() {
    switch (g_state) {
        case GameState::Loading:
            g_loading_screen.render(g_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
            break;
        case GameState::Intro:
            // Black background for intro
            SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
            SDL_RenderClear(g_renderer);
            g_dialogue_system.render(g_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
            break;
        case GameState::Title:
            g_title_screen.render(g_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
            break;
        case GameState::Playing:
            // Nothing
            break;
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
