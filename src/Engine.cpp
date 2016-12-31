#include "Engine.h"

#include <thread>
#include <chrono>

#if SH_CAMERA && defined(WIN32)
#include <SDL.h>
#endif

Engine::Engine( ImageSource::Ptr&& pImgSrc, ImageProcessor::Ptr&& pImgProc ) :
	m_pImageSource( std::move( pImgSrc ) ),
	m_pImageProcessor( std::move( pImgProc ) )
{}

void Engine::Run()
{
	if ( !( m_pImageSource && m_pImageProcessor ) )
		throw std::runtime_error( "Error: Engine not initialized" );

	// The EDSDK needs a window to send messages to
#if SH_CAMERA && defined(WIN32)
    SDL_Window * pWindow = SDL_CreateWindow( "EDSDK Dummy Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 300, 300, SDL_WINDOW_SHOWN );
#endif

    // Init image source / processor
    m_pImageSource->Initialize();
    m_pImageProcessor->Initialize();

    // Iterate over all images
	bool bQuitFlag( false );
    using ImgStat = ImageSource::Status;
	img_t img;
    for ( ImgStat st = m_pImageSource->GetNextImage( &img ); st != ImgStat::DONE && !bQuitFlag; m_pImageSource->GetNextImage( &img ) )
    {
        if (st == ImgStat::WAIT)
        {
#if SH_CAMERA && defined(WIN32)
            SDL_Event e { 0 };
			while ( SDL_PollEvent( &e ) )
			{
				if ( e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_ESCAPE )
				{
					bQuitFlag = true;
				}
			}
#endif

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        else if (st == ImgStat::READY){
			m_pImageProcessor->HandleImage( img );
        }
    }
#if SH_CAMERA && defined(WIN32)
    SDL_DestroyWindow( pWindow );
#endif

    // Finalize source / processor
	m_pImageProcessor->Finalize();
    m_pImageSource->Finalize();
}