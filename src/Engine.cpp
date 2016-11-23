#include "Engine.h"

#include <thread>
#include <chrono>

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
    ImageSource::Status st = m_pImageSource->GetStatus();
    for ( ; st != ImageSource::Status::DONE; st = m_pImageSource->GetStatus()){
        if (st == ImageSource::Status::WAIT){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        else if (st == ImageSource::Status::READY){
            m_pImageProcessor->HandleImage(m_pImageSource->GetNextImage());
        }
    }

    // Finalize source / processor
	m_pImageProcessor->Finalize();
    m_pImageSource->Finalize();
}
