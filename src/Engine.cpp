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


#if SH_CAMERA && defined(WIN32)
    SDL_Window * pWindow = SDL_CreateWindow( "fuck", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 300, 300, SDL_WINDOW_HIDDEN );
#endif

    // Init image source / processor
    m_pImageSource->Initialize();
    m_pImageProcessor->Initialize();

    // Iterate over all images
    using ImgStat = ImageSource::Status;
    for ( ImgStat st = m_pImageSource->GetStatus(); st != ImgStat::DONE; st = m_pImageSource->GetStatus())
    {
        if (st == ImgStat::WAIT)
        {

#if SH_CAMERA && defined(WIN32)
            SDL_Event e { 0 };
            while(SDL_PollEvent( &e ))
                std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
#endif

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        else if (st == ImgStat::READY){
            m_pImageProcessor->HandleImage(m_pImageSource->GetNextImage());
        }
    }


#if SH_CAMERA && defined(WIN32)
    SDL_DestroyWindow( pWindow );
#endif

    // Finalize source / processor
	m_pImageProcessor->Finalize();
    m_pImageSource->Finalize();
}
