#include "FileReader.h"

#include <algorithm>

ImageSource::Status FileReader::GetStatus() const
{
	return m_liFileNames.empty() ? ImageSource::Status::DONE : ImageSource::Status::READY;
}

cv::Mat FileReader::GetNextImage()
{
	if ( GetStatus() == ImageSource::Status::DONE )
		throw std::runtime_error( "Error: FileReader has no more images!" );
	
	cv::Mat matRet = cv::imread( m_liFileNames.front() );
	m_liFileNames.pop_front();

	return matRet;
}

void FileReader_WithDrift::IncDriftVel( int nDriftX, int nDriftY )
{
    m_nDriftVelX += nDriftX;
    m_nDriftVelY += nDriftY;
}

void FileReader_WithDrift::SetDriftVel( int nDriftX, int nDriftY )
{
    m_nDriftVelX = nDriftX;
    m_nDriftVelY = nDriftY;
}

void FileReader_WithDrift::SetOffset( int nOfsX, int nOfsY )
{
    m_nOfsX = nOfsX;
    m_nOfsY = nOfsY;
}

void FileReader_WithDrift::GetDriftVel( int * pnDriftX, int * pnDriftY ) const
{
    if ( pnDriftX )
        *pnDriftX = m_nDriftVelX;
    if ( pnDriftY )
        *pnDriftY = m_nDriftVelY;
}

void FileReader_WithDrift::GetOffset(int * pnOfsX, int * pnOfsY) const{
    if (pnOfsX)
        *pnOfsX = m_nOfsX;
    if (pnOfsY)
        *pnOfsY = m_nOfsY;
}

cv::Mat FileReader_WithDrift::GetNextImage()
{
    // Get next image
    cv::Mat img = FileReader::GetNextImage();

    // Update offset value
    m_nOfsX += m_nDriftVelX;
    m_nOfsY += m_nDriftVelY;

    // Return if no offset
    if ( !( m_nOfsX || m_nOfsY ) )
        return img;
    
    // std::cout << m_nOfsX << ", " << m_nOfsY << std::endl;

    // Do the translation by moving a sub-image
    // at an offset into a new mat
    cv::Rect rcSrc, rcDst;

    // X position, left is 0
    rcSrc.x = m_nOfsX < 0 ? -m_nOfsX : 0;
    rcSrc.width = img.cols - abs( m_nOfsX );
    rcDst.x = -m_nOfsX - rcSrc.x;
    rcDst.width = rcSrc.width;

    // Y position, top is 0 (so flip)
    rcSrc.y = m_nOfsY > 0 ? m_nOfsY : 0;
    rcSrc.height = img.cols - abs( m_nOfsY );
    rcDst.y = rcSrc.y - m_nOfsY;
    rcDst.height = rcSrc.height;

    // Copy sub image src into zeroed out dst
    cv::Mat ret( cv::Mat::zeros( img.size(), img.type() ) );
    cv::Mat subImg = img( rcSrc );
    subImg.copyTo( ret( rcDst ) );

    return ret;
}