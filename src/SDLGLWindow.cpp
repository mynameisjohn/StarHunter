#include "SDLGLWindow.h"

#include <GL/glew.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include <iostream>

SDLGLWindow::SDLGLWindow( std::string strName, int posX, int posY, int width, int height, int flags,
						  int glMajor, int glMinor, bool bDoubleBuf )
{
	SDL_Window * pWindow( nullptr );
	SDL_GLContext glContext( nullptr );
	pWindow = SDL_CreateWindow( strName.c_str(),
								posX,
								posY,
								width,
								height,
								flags );

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, glMajor );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, glMinor );

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, bDoubleBuf ? 1 : 0 );

	glContext = SDL_GL_CreateContext( pWindow );
	if ( glContext == nullptr )
	{
		throw std::runtime_error( "Error creating opengl context" );
	}

	//Initialize GLEW
	glewExperimental = GL_TRUE;
	GLenum glewError = glewInit();
	if ( glewError != GLEW_OK )
	{
		throw std::runtime_error( "Error initializing GLEW! " + std::string( (char *) glewGetErrorString( glewError ) ) );
	}

	SDL_GL_SetSwapInterval( 1 );

	glClearColor( 0, 0, 0, 1 );

	glEnable( GL_DEPTH_TEST );
	glDepthMask( GL_TRUE );
	glDepthFunc( GL_LESS );
	glEnable( GL_MULTISAMPLE_ARB );

	if ( pWindow )
		m_pWindow = pWindow;
	if ( glContext )
		m_GLContext = glContext;
}

SDLGLWindow::~SDLGLWindow()
{
	if ( m_pWindow )
		SDL_DestroyWindow( m_pWindow );
	if ( m_GLContext )
		SDL_GL_DeleteContext( m_GLContext );
}

SDL_Window * SDLGLWindow::GetWindow() const
{
	return m_pWindow;
}

SDLGLWindow::Updater::Updater( SDL_Window * pW ) :
	pWND( pW )
{
	if ( pWND )
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
}

SDLGLWindow::Updater::~Updater()
{
	if ( pWND )
		SDL_GL_SwapWindow( pWND );
}