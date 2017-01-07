#pragma once

#include <string>

struct SDL_Window;
typedef void* SDL_GLContext;

// Class I wrote to make projects with SDL OpenGL backed windows
// This will actually create an OpenGL context and construct a window
class SDLGLWindow
{
	// Owns window pointer and context
	SDL_Window * m_pWindow;
	SDL_GLContext m_GLContext;

public:
	// Create openGL context with options, construct SDL window
	// Will throw runtime error if something fails
	SDLGLWindow( std::string strName, int posX, int posY, int width, int height, int flags,
				 int glMajor, int glMinor, bool bDoubleBuf );

	// Destroys window and context
	virtual ~SDLGLWindow();

	// Return window pointer
	SDL_Window * GetWindow() const;

	// Return context pointer
	SDL_GLContext GetContext() const;

	virtual void Draw() = 0;

	// Scoped updater class, 
	// calls clear color on construction
	// and swaps window on destruction
	struct Updater
	{
		SDL_Window * pWND;
		Updater( SDL_Window * pW );
		~Updater();
	};
};
