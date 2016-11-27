#include "Engine.h"

#include <thread>
#include <chrono>

#include <libraw.h>

Engine::Engine( ImageSource::Ptr&& pImgSrc, ImageProcessor::Ptr&& pImgProc ) :
	m_pImageSource( std::move( pImgSrc ) ),
	m_pImageProcessor( std::move( pImgProc ) )
{}

void Engine::Run()
{
	if ( !( m_pImageSource && m_pImageProcessor ) )
		throw std::runtime_error( "Error: Engine not initialized" );

    // Init image source / processor
    m_pImageSource->Initialize();
    m_pImageProcessor->Initialize();

    // Iterate over all images
    using ImgStat = ImageSource::Status;
    for ( ImgStat st = m_pImageSource->GetStatus(); st != ImgStat::DONE; st = m_pImageSource->GetStatus()){
        if (st == ImgStat::WAIT){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        else if (st == ImgStat::READY){
            m_pImageProcessor->HandleImage(m_pImageSource->GetNextImage());
        }
    }

    // Finalize source / processor
	m_pImageProcessor->Finalize();
    m_pImageSource->Finalize();
}
