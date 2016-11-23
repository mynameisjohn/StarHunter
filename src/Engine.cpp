#include "Engine.h"

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

    // Iterate over all images (TODO better status enumeration, i.e skip vs break)
	while ( m_pImageSource->HasImages() )
		if ( m_pImageProcessor->HandleImage( std::move( m_pImageSource->GetNextImage() ) ) == false )
			break;

    // Finalize source / processor
	m_pImageProcessor->Finalize();
    m_pImageSource->Finalize();
}
