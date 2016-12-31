#include "FileReader.h"
#include "Util.h"

#include <algorithm>

#if SH_CAMERA
#include <libraw/libraw.h>
#endif // SH_CAMERA

ImageSource::Status FileReader::GetNextImage( img_t * pImg )
{
	if ( m_liFileNames.empty() )
		return ImageSource::Status::DONE;

    // Get the first file name and pop it off
    std::string strFileName = std::move( m_liFileNames.front() );
	m_liFileNames.pop_front();

    // Load the image (handle png and raw separately)
    size_t ixDot = strFileName.find_last_of( "." );
    if ( ixDot != std::string::npos && ixDot < strFileName.size() - 1 )
    {
        std::string strExt = strFileName.substr( ixDot + 1 );
		if ( strExt == "png" )
		{
#if SH_CUDA
			cv::Mat imgPng_h = cv::imread( strFileName );
			img_t imgPng;
			imgPng.upload( imgPng_h );
#else
			img_t imgPng = cv::imread( strFileName );
#endif
			if ( !imgPng.empty() )
			{
				img_t imgRet;
				const double dDivFactor = 1. / ( 1 << ( 8 * imgPng.elemSize() / imgPng.channels() ) );
				if ( imgPng.channels() > 1 )
				{
					img_t imgPngGray;
					::cvtColor( imgPng, imgPngGray, CV_RGB2GRAY );
					imgPngGray.convertTo( imgRet, CV_32FC1, dDivFactor );
				}
				else
				{
					imgPng.convertTo( imgRet, CV_32FC1, dDivFactor );
				}

				*pImg = imgRet;
				return Status::READY;
			}
		}
#if SH_CAMERA
		else if ( strExt == "cr2" )
		{
			*pImg = Raw2Img( strFileName );
			return Status::READY;
		}
#endif
    }

    // We should have handled it
    throw std::runtime_error( "Error: FileReader unable to load image!" );
	return Status::DONE;
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

ImageSource::Status FileReader_WithDrift::GetNextImage( img_t * pImg )
{
    // Get next image
	img_t img;
	if ( FileReader::GetNextImage( &img ) == Status::DONE )
		return Status::DONE;

    // Update offset value
    m_nOfsX += m_nDriftVelX;
    m_nOfsY += m_nDriftVelY;

    // Return if no offset
	if ( !( m_nOfsX || m_nOfsY ) )
	{
		*pImg = img;
		return Status::READY;
	}
    
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
    img_t ret( cv::Mat::zeros( img.size(), img.type() ) );
    img_t subImg = img( rcSrc );
    subImg.copyTo( ret( rcDst ) );
	
	*pImg = ret;
    return Status::READY;
}

#ifndef SH_CUDA
img_t GetBayerData( int width, int height, uint16_t * pData )
{
    // Compute area
	int area = width * height;

	// Create a buffer of ushorts containing the pixel values of the
	// "BG Bayered" image (even rows are RGRGRG..., odd are GBGBGB...)'
	std::vector<uint16_t> vBayerDataBuffer( area );

	// Pull the data out of the image
#pragma omp parallel for
	for ( int idx = 0; idx < area; idx++ )
	{
		// Get the color data - LibRaw gives me 4 shorts, and
		// only one is nonzero (the bayer component I want at idx)
		// To avoid a branch I'll just take them all
		vBayerDataBuffer[idx] += pData[4 * idx + 0];
		vBayerDataBuffer[idx] += pData[4 * idx + 1];
		vBayerDataBuffer[idx] += pData[4 * idx + 2];
		vBayerDataBuffer[idx] += pData[4 * idx + 3];

		// To say this isn't a fudge factor would be a lie - I think the bit
		// depth of my camera is 14 and I want 16, but I really don't know
		vBayerDataBuffer[idx] <<= 2;

		// Can I convert to float here? I'm not sure if cvtColor can handle it
		// but I don't see why not, and I can debayer the image myself
		//float fVal = vBayerDataBuffer[idx] / float( 1 << 14 );

		// I wonder if I can threshold here without a branch as well
		// If I have a 14 bit range, cutting off the last 11 bits would
		// be equivalent to thresholding a float at 0.125f
		//const uint16_t trunc = vBayerDataBuffer[idx] & 0xC000;
		//const uint16_t rem = vBayerDataBuffer[idx] & 0x07ff;
	}

    // Construct a mat from the data and return it
	return cv::Mat( vBayerDataBuffer, CV_16UC1 ).reshape( 1, height );
}
#endif

#if SH_CAMERA
// Convert some LibRaw object to a image type
img_t Raw2Img_impl( LibRaw& lrProc, bool bRecycle = true )
{
    // Get image dimensions
    int width = lrProc.imgdata.sizes.iwidth;
    int height = lrProc.imgdata.sizes.iheight;

	// Get the bayered data
	img_t imgBayer = GetBayerData( width, height, (uint16_t *) lrProc.imgdata.image );

	// Get rid of libraw image if requested
	if ( bRecycle )
		lrProc.recycle();

	// Debayer the image to 16-bit gray
	img_t imgDeBayerGrayU16;
	::cvtColor( imgBayer, imgDeBayerGrayU16, CV_BayerBG2GRAY );

    // Make normalized float
	img_t imgDeBayerGrayF32;
    const double dDivFactor= 1. / ( 1 << ( 8 * imgDeBayerGrayU16.elemSize() ) );
	imgDeBayerGrayU16.convertTo( imgDeBayerGrayF32, CV_32FC1, dDivFactor );

	// Threshold (in place?)
	const double dThresh = .15;
	::threshold( imgDeBayerGrayF32, imgDeBayerGrayF32, dThresh, 0, CV_THRESH_TOZERO );

	//displayImage( "Test CR2", imgDeBayerGrayF32 );

	// Return float image
    return imgDeBayerGrayF32;
}

img_t Raw2Img( void * pData, size_t uNumBytes )
{
    // Open the CR2 file with LibRaw, unpack, and create image
    LibRaw lrProc;
    assert( LIBRAW_SUCCESS == lrProc.open_buffer( pData, uNumBytes ) );
    assert( LIBRAW_SUCCESS == lrProc.unpack() );
    assert( LIBRAW_SUCCESS == lrProc.raw2image() );

    return Raw2Img_impl( lrProc );
}

img_t Raw2Img( std::string strFileName )
{
    // Open the CR2 file with LibRaw, unpack, and create image
    LibRaw lrProc;
    assert( LIBRAW_SUCCESS == lrProc.open_file( strFileName.c_str() ) );
    assert( LIBRAW_SUCCESS == lrProc.unpack() );
    assert( LIBRAW_SUCCESS == lrProc.raw2image() );

    return Raw2Img_impl( lrProc );
}
#endif
