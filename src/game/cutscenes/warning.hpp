#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <string>

#ifndef NO_AUDIO
#include <SDL_mixer_ext.h>
#endif

class WarningScreen {
public:
	WarningScreen();
	~WarningScreen();

	bool init(const std::string& fontPath, int fontSize = 24);
	void cleanup();

	void reset();
	void update(Uint32 delta_time);
	void render(SDL_Renderer* renderer, int screen_width, int screen_height);
	void handleKeyDown(SDL_Keycode key);

	bool isComplete() const { return m_complete; }

private:
	void renderCenteredText(SDL_Renderer* renderer,
							const std::string& text,
							int centerX,
							int y,
							SDL_Color color,
							Uint32 wrapWidth) const;

	TTF_Font* m_font = nullptr;

#ifndef NO_AUDIO
	Mix_Music* m_voiceClip = nullptr;
#endif

	Uint32 m_elapsedTime = 0;
	bool m_inputEnabled = false;
	bool m_voiceStarted = false;
	bool m_complete = false;
};