#pragma once

#include <string>

struct SDL_Window;
typedef void* SDL_GLContext;

class SDLGLWindow
{
	SDL_Window * m_pWindow;
	SDL_GLContext m_GLContext;

public:
	SDLGLWindow( std::string strName, int posX, int posY, int width, int height, int flags,
				 int glMajor, int glMinor, bool bDoubleBuf );
	~SDLGLWindow();

	SDL_Window * GetWindow() const;

	struct Updater
	{
		SDL_Window * pWND;
		Updater( SDL_Window * pW );
		~Updater();
	};
};
