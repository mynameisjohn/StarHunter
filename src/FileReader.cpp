#include "FileReader.h"
#include "Util.h"

#include <algorithm>

#if SH_CAMERA
#include <libraw/libraw.h>
#endif // SH_CAMERA

ImageSource::Status FileReader::GetStatus() const
{
	return m_liFileNames.empty() ? ImageSource::Status::DONE : ImageSource::Status::READY;
}

cv::Mat FileReader::GetNextImage()
{
    // Don't do this if done
	if ( GetStatus() == ImageSource::Status::DONE )
		throw std::runtime_error( "Error: FileReader has no more images!" );
	
    // Get the first file name and pop it off
    std::string strFileName = std::move( m_liFileNames.front() );
	m_liFileNames.pop_front();

    // Load the image (handle png and raw separately)
    size_t ixDot = strFileName.find_last_of( "." );
    if ( ixDot != std::string::npos && ixDot < strFileName.size() - 1 )
    {
        std::string strExt = strFileName.substr( ixDot + 1 );
        if ( strExt == "png" )
            return cv::imread( m_liFileNames.front() );
#if SH_CAMERA
        else if ( strExt == "cr2" )
            return Raw2Mat( strFileName );
#endif // SH_CAMERA
    }

    // We should have handled it
    throw std::runtime_error( "Error: FileReader unable to load image!" );
	return cv::Mat();
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

#if SH_CAMERA
cv::Mat Raw2Mat_impl( LibRaw& lrProc, bool bRecycle = true )
{
    // Get image dimensions
    int width = lrProc.imgdata.sizes.iwidth;
    int height = lrProc.imgdata.sizes.iheight;

    // Create a buffer of ushorts containing the pixel values of the
    // "BG Bayered" image (even rows are RGRGRG..., odd are GBGBGB...)'
    std::vector<uint16_t> vBayerDataBuffer;
    if ( vBayerDataBuffer.empty() )
        vBayerDataBuffer.resize( width*height );
    else
        std::fill( vBayerDataBuffer.begin(), vBayerDataBuffer.end(), 0 );

    std::vector<uint16_t>::iterator itBayer = vBayerDataBuffer.begin();
    for ( int y = 0; y < height; y++ )
    {
        for ( int x = 0; x < width; x++ )
        {
            // Get pixel idx
            int idx = y * width + x;

            // Each pixel is an array of 4 shorts rgbg
            ushort * uRGBG = lrProc.imgdata.image[idx];
            int rowMod = ( y ) % 2; // 0 if even, 1 if odd
            int colMod = ( x + rowMod ) % 2; // 1 if even, 0 if odd
            int clrIdx = 2 * rowMod + colMod;
            ushort val = uRGBG[clrIdx] << 4;
            *itBayer++ = val;
        }
    }

    // Get rid of libraw image, construct openCV mat
    lrProc.recycle();
    cv::Mat imgBayer( height, width, CV_16UC1, vBayerDataBuffer.data() );

    // Debayer image, get output
    cv::Mat imgDeBayer;
    cv::cvtColor( imgBayer, imgDeBayer, CV_BayerBG2BGR );

    assert( imgDeBayer.type() == CV_16UC3 );

    // Convert to grey
    cv::Mat imgGrey;
    cv::cvtColor( imgDeBayer, imgGrey, cv::COLOR_RGB2GRAY );

    // Make normalized float
    cv::Mat imgTmp = imgGrey; 
    const double div = ( 1 << ( 8 * imgGrey.elemSize() ) );
    imgTmp.convertTo( imgGrey, CV_32FC1, 1. / div );
    
    return imgGrey;
}

cv::Mat Raw2Mat( void * pData, size_t uNumBytes )
{
    // Open the CR2 file with LibRaw, unpack, and create image
    LibRaw lrProc;
    assert( LIBRAW_SUCCESS == lrProc.open_buffer( pData, uNumBytes ) );
    assert( LIBRAW_SUCCESS == lrProc.unpack() );
    assert( LIBRAW_SUCCESS == lrProc.raw2image() );

    return Raw2Mat_impl( lrProc );
}

cv::Mat Raw2Mat( std::string strFileName )
{
    // Open the CR2 file with LibRaw, unpack, and create image
    LibRaw lrProc;
    assert( LIBRAW_SUCCESS == lrProc.open_file( strFileName.c_str() ) );
    assert( LIBRAW_SUCCESS == lrProc.unpack() );
    assert( LIBRAW_SUCCESS == lrProc.raw2image() );

    return Raw2Mat_impl( lrProc );
}

#endif // SH_CAMERA
