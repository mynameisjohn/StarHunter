#pragma once

#include "Engine.h"
#include <list>
#include <initializer_list>

// FileReader class, reads in a list of image files and streams them
class FileReader : public ImageSource
{
	std::list<std::string> m_liFileNames;
public:
	//FileReader( std::initializer_list<std::string> liFileNames ) : m_liFileNames( liFileNames ) {}
    template<typename C>
    FileReader( C liFileNames ) : m_liFileNames( liFileNames.begin(), liFileNames.end() ) {}

	ImageSource::Status GetNextImage( img_t * pImg ) override;
};

// Like above, but a pixel offset can be applied
// to images (meant to simulate a moving camera)
class FileReader_WithDrift : public FileReader{
    int m_nOfsX;
    int m_nOfsY;
    int m_nDriftVelX;
    int m_nDriftVelY;
public:
    template<typename C>
    FileReader_WithDrift( C liFileNames ) :
        FileReader( liFileNames ),
        m_nOfsX( 0 ),
        m_nOfsY( 0 ),
        m_nDriftVelX( 0 ),
        m_nDriftVelY( 0 )
    {}

	ImageSource::Status GetNextImage( img_t * pImg ) override;
    void SetDriftVel(int pnDriftX, int pnDriftY);
    void IncDriftVel( int pnDriftX, int pnDriftY );
    void GetDriftVel( int * pnDriftX, int * pnDriftY ) const;
    void SetOffset( int nOfsX, int nOfsY );
    void GetOffset(int * pnOfsX, int * pnOfsY) const;
};

// Defined in a kernel for cuda
img_t GetBayerData( int width, int height, uint16_t * pData );