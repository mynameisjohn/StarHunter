#include "StarFinder.h"
#include "FileReader.h"
#include "Util.h"

#include <opencv2/opencv.hpp>

// Filtering functions, defined below
void DoGaussianFilter( const int nFilterRadius, const double dSigma, img_t& input, img_t& output );
void DoTophatFilter( const int nFilterRadius, img_t& input, img_t& output );
void DoDilationFilter( const int nFilterRadius, img_t& input, img_t& output );

// Arbitrarily small number
const double kEPS = .001;

StarFinder::StarFinder() :
	// These are some good defaults
	m_fFilterRadius( .03f ),
	m_fDilationRadius( .015f ),
	m_fHWHM( 2.5f ),
	m_fIntensityThreshold( 0.25f )
{}

bool StarFinder::findStars( img_t& img )
{
	if ( img.empty() )
		return false;

	// So we know what we're working with here
    if ( img.type() != CV_32F )
		throw std::runtime_error( "Error: What kind of image is StarFinder working with?!" );

	// Initialize if we haven't yet
	if ( m_imgInput.empty() )
	{
		// Preallocate the GPU mats needed during computation
		m_imgInput = img;
		m_imgGaussian = img_t( img.size(), CV_32F );
		m_imgTopHat = img_t( img.size(), CV_32F );
		m_imgPeak = img_t( img.size(), CV_32F );
		m_imgThreshold = img_t( img.size(), CV_32F );
		m_imgDilated = img_t( img.size(), CV_32F );
		m_imgLocalMax = img_t( img.size(), CV_32F );
		m_imgStars = img_t( img.size(), CV_32F );

        // We need a contiguous boolean image for CUDA
#if SH_CUDA
        m_imgBoolean = cv::cuda::createContinuous( img.size(), CV_8U );
#else
        m_imgBoolean = img_t( img.size(), CV_8U );
#endif
	}

	// I think I can just steal the reference
//	// Upload input to GPU for CUDA
//#if SH_CUDA
//    m_imgInput.upload( img );
//#else
//    m_imgInput = img.clone();
//#endif

	int nFilterRadius = (int) ( .5f + m_fFilterRadius * m_imgInput.cols );
	int nDilationRadius = (int) ( .5f + m_fDilationRadius * m_imgInput.cols );

	// Apply gaussian filter to input to remove high frequency noise
    const double dSigma = m_fHWHM / ( ( sqrt( 2 * log( 2 ) ) ) );
    DoGaussianFilter( nFilterRadius, dSigma, m_imgInput, m_imgGaussian );

	// Apply linear filter to input to magnify high frequency noise
    DoTophatFilter( nFilterRadius, m_imgInput, m_imgTopHat );

	// Subtract linear filtered image from gaussian image to clean area around peak
	// Noisy areas around the peak will be negative, so threshold negative values to zero
	::subtract( m_imgGaussian, m_imgTopHat, m_imgPeak);
	::threshold( m_imgPeak, m_imgPeak, 0, 1, cv::THRESH_TOZERO );

	// Create a thresholded image where the lowest pixel value is m_fIntensityThreshold
	m_imgThreshold.setTo( cv::Scalar( m_fIntensityThreshold ) );
	::max( m_imgPeak, m_imgThreshold, m_imgThreshold );

	// Create the dilated image (initialize its pixels to m_fIntensityThreshold)
	m_imgDilated.setTo( cv::Scalar( m_fIntensityThreshold ) );
	
    DoDilationFilter( nDilationRadius, m_imgThreshold, m_imgDilated );

	// Subtract the dilated image from the gaussian peak image
	// What this leaves us with is an image where the brightest
	// gaussian peak pixels are zero and all others are negative
	::subtract( m_imgPeak, m_imgDilated, m_imgLocalMax );

	// Exponentiating that image makes those zero pixels 1, and the
	// negative pixels some low number; threshold to drop them
	::exp( m_imgLocalMax, m_imgLocalMax );
	::threshold( m_imgLocalMax, m_imgStars, 1 - kEPS, 1 + kEPS, cv::THRESH_BINARY );

	// This star image is now a boolean image - convert it to bytes (TODO you should add some noise)
    m_imgStars.convertTo( m_imgBoolean, CV_8U, 0xff );

	return true;
}

// Just find the stars
bool StarFinder::HandleImage( img_t img )
{
	return findStars( img );
}

StarFinder_UI::StarFinder_UI() :
	StarFinder()
{}

bool StarFinder_UI::HandleImage( img_t img )
{
	// Call this once to test input and init images
	if ( !findStars( img ) )
		return false;

	// Create window, trackbars
	////////////////////////////////////////////////////
	// Window name
	const std::string strWindowName = "Star Finder";
	cv::namedWindow( strWindowName, cv::WINDOW_AUTOSIZE );

	const int nTrackBarRes = 1000;
	std::map<std::string, int> mapParamValues = {
		{ "Filter Radius", nTrackBarRes *  m_fFilterRadius  },
		{ "Dilation Radius", nTrackBarRes *  m_fDilationRadius },
		{ "Intensity Threshold", nTrackBarRes * m_fIntensityThreshold },
		{ "FWHM", nTrackBarRes *  m_fHWHM }
	};

	// Trackbar Callback
	// Trackbar callback, implemented below
	std::function<void( int, void * )> trackBarCallback = [&, this]( int pos, void * priv )
	{
		// Set parameters
		m_fFilterRadius = float( mapParamValues["Filter Radius"] ) / nTrackBarRes;
		m_fDilationRadius = float( mapParamValues["Dilation Radius"] ) / nTrackBarRes;
		m_fIntensityThreshold = float( mapParamValues["Intensity Threshold"] ) / nTrackBarRes;
		m_fHWHM = float( mapParamValues["FWHM"] ) / nTrackBarRes;

		// Find stars (results are in member images)
		if ( findStars( img ) == false )
			throw std::runtime_error( "Error! Why did findStars return false?" );

		// Use thrust to find stars in pixel coordinates
		const float fStarRadius = 10.f;
		std::vector<Circle> vStarLocations = FindStarsInImage( fStarRadius, m_imgBoolean );

		// Create copy of original input and draw circles where stars were found
#if SH_CUDA
		cv::Mat hHighlight;
		img.download( hHighlight );
#else
		cv::Mat hHighlight = img.clone();
#endif
		const int nHighlightThickness = 1;
		const cv::Scalar sHighlightColor( 0xde, 0xad, 0 );
		
		for ( const Circle ptStar : vStarLocations )
		{
			// TODO should I be checking if we go too close to the edge?
			cv::circle( hHighlight, cv::Point( ptStar.fX, ptStar.fY ), ptStar.fR, sHighlightColor, nHighlightThickness );
		}

		// Show the image with circles
		cv::resizeWindow( strWindowName, hHighlight.size().width, hHighlight.size().height );
		cv::imshow( strWindowName, hHighlight );
	};

	// Create trackbars
	for ( std::string strTrackBarName : {"Filter Radius", "Dilation Radius", "Intensity Threshold"} )
		cv::createTrackbar( strTrackBarName, strWindowName, &mapParamValues[strTrackBarName], nTrackBarRes, get_fn_ptr<0>( trackBarCallback ) );
	cv::createTrackbar( "FWHM", strWindowName, &mapParamValues["FWHM"], 10 * nTrackBarRes, get_fn_ptr<0>( trackBarCallback ) );

	// Call the callback once to initialize the window
	trackBarCallback( 0, nullptr );

	// Wait while user sets things until they press a key (any key?)
	cv::waitKey();

	return true;
}

StarFinder_Drift::StarFinder_Drift() :
    StarFinder(),
    m_nImagesProcessed( 0 ),
    m_fDriftX_Prev( 0 ),
    m_fDriftY_Prev( 0 ),
	m_fDriftX_Cumulative( 0 ),
	m_fDriftY_Cumulative( 0 )
{}

bool StarFinder_Drift::HandleImage( img_t img )
{
	if ( !findStars( img ) )
		return false;

	if ( m_vLastCircles.empty() )
	{
		// Store if not yet created
        const float fStarRadius = 10.f;
		m_vLastCircles = FindStarsInImage( fStarRadius, m_imgBoolean );
	}
	else
	{
		// Use thrust to find stars in pixel coordinates
		std::vector<Circle> vStarLocations = FindStarsInImage( 10, m_imgBoolean );

		// If these are sized different, we have problems
		//if ( vStarLocations.size() != m_vLastCircles.size() )
		//	throw std::runtime_error( "We lost some stars" );

		// Compute the average drift for this set of matches
		float fDriftAvgX = 0;
		float fDriftAvgY = 0;

		// For every circle in our last vector
		for ( const Circle cOld : m_vLastCircles )
		{
			// Try to find a match in this vector
			bool bMatchFound = false;
			for ( const Circle cNew : vStarLocations )
			{
				// Compute the drift between old and new
				float fDistX = cNew.fX - cOld.fX;
				float fDistY = cNew.fY - cOld.fY;
				float fDist2 = pow( fDistX, 2 ) + pow( fDistY, 2 );
				if ( fDist2 < pow( cOld.fR + cNew.fR, 2 ) )
				{
					// For now I'd like to ensure that we don't have duplicate matches
					if ( bMatchFound )
						throw std::runtime_error( "Error: Why were there two matches?" );
					bMatchFound = true;

					// Divide this drift by the # of stars we have to average
					fDriftAvgX += fDistX / float( m_vLastCircles.size() );
					fDriftAvgY += fDistY / float( m_vLastCircles.size() );
				}
			}
		}

		// Update cached positions, inc cumulative drift counter
		m_vLastCircles = std::move( vStarLocations );
        m_fDriftX_Prev = fDriftAvgX;
        m_fDriftY_Prev = fDriftAvgY;
		m_fDriftX_Cumulative += fDriftAvgX;
		m_fDriftY_Cumulative += fDriftAvgY;
		m_nImagesProcessed++;
	}

	return true;
}

bool StarFinder_Drift::GetDrift_Prev( float * pDriftX, float * pDriftY ) const
{
    // Nothing to average yet
    if ( !( m_nImagesProcessed && pDriftX && pDriftY ) )
        return false;

    *pDriftX = m_fDriftX_Prev;
    *pDriftY = m_fDriftY_Prev;

    return true;
}

bool StarFinder_Drift::GetDrift_Cumulative( float * pDriftX, float * pDriftY ) const
{
	// Nothing to average yet
	if ( !( m_nImagesProcessed && pDriftX && pDriftY ) )
		return false;

	// Divide cumulative drift by # of images it was computed from
	*pDriftX = m_fDriftX_Cumulative / float( m_nImagesProcessed );
	*pDriftY = m_fDriftY_Cumulative / float( m_nImagesProcessed );

	return true;
}

//bool StarFinder_Drift::GetDriftN( float * pDriftX, float * pDriftY ) const
//{
//    // Nothing to average yet
//	if ( !( m_nImagesProcessed && pDriftX && pDriftY ) )
//		return false;
//
//	// Normalize drift values
//    float fMag = sqrt(pow(m_fDriftX_Cumulative, 2) + pow(m_fDriftY_Cumulative, 2));
//    *pDriftX = m_fDriftX_Cumulative / fMag;
//    *pDriftY = m_fDriftX_Cumulative / fMag;
//
//	return true;
//}

StarFinder_ImgOffset::StarFinder_ImgOffset( FileReader_WithDrift * pFileReader):
    StarFinder_Drift(),
    m_pFileReader(pFileReader)
{}

bool StarFinder_ImgOffset::HandleImage( img_t img ) 
{ 
    // displayImage( "Offset", img );

    // Do default behavior
	if ( StarFinder_Drift::HandleImage( img ) )
	{
        // If we have a filereader
		if ( m_pFileReader )
        {
            // Get drift values (returns false if < 2 images processed)
            float fDriftX(0), fDriftY(0);
            if ( GetDrift_Prev( &fDriftX, &fDriftY ) )
            {
                // Increment drift of FR velocity by current amount
                int nDriftX = (int) fDriftX;
                int nDriftY = (int) fDriftY;
                m_pFileReader->IncDriftVel( -nDriftX, nDriftY );
            }
        }

        return true;
    }

    return false;
}

void displayImage( std::string strWindowName, cv::Mat& img )
{
	cv::namedWindow( strWindowName, CV_WINDOW_FREERATIO );
	cv::imshow( strWindowName, img );
	cv::waitKey();
	cv::destroyWindow( strWindowName );
}

#if SH_CUDA
void displayImage( std::string strWindowName, cv::cuda::GpuMat& img )
{
    cv::namedWindow( strWindowName, CV_WINDOW_OPENGL );
    cv::imshow( strWindowName, img );
    cv::waitKey();
    cv::destroyWindow( strWindowName );
}

void DoTophatFilter( const int nFilterRadius, img_t& input, img_t& output )
{
    int nDiameter = 2 * nFilterRadius + 1;
    cv::Mat h_Circle = cv::Mat::zeros( cv::Size( nDiameter, nDiameter ), CV_32F );
    cv::circle( h_Circle, cv::Size( nFilterRadius, nFilterRadius ), nFilterRadius, 1.f, -1 );
    h_Circle /= cv::sum( h_Circle )[0];
    cv::Ptr<cv::cuda::Filter> pLinCircFilter = cv::cuda::createLinearFilter( CV_32F, CV_32F, h_Circle );
    pLinCircFilter->apply( input, output );
}

void DoGaussianFilter( const int nFilterRadius, const double dSigma, img_t& input, img_t& output )
{
    int nDiameter = 2 * nFilterRadius + 1;
    cv::Ptr<cv::cuda::Filter> pGaussFilter = cv::cuda::createGaussianFilter( CV_32F, CV_32F, cv::Size( nDiameter, nDiameter ), dSigma );
    pGaussFilter->apply( input, output );
}

void DoDilationFilter( const int nFilterRadius, img_t& input, img_t& output )
{
    int nDilationDiameter = 2 * nFilterRadius + 1;
    cv::Mat hDilationStructuringElement = cv::getStructuringElement( cv::MORPH_ELLIPSE, cv::Size( nDilationDiameter, nDilationDiameter ) );
    cv::Ptr<cv::cuda::Filter> pDilation = cv::cuda::createMorphologyFilter( cv::MORPH_DILATE, CV_32F, hDilationStructuringElement );
    pDilation->apply( input, output );
}
#else
void DoTophatFilter( const int nFilterRadius, img_t& input, img_t& output )
{
    int nDiameter = 2 * nFilterRadius + 1;
    cv::Mat h_Circle = cv::Mat::zeros( cv::Size( nDiameter, nDiameter ), CV_32F );
    cv::circle( h_Circle, cv::Size( nFilterRadius, nFilterRadius ), nFilterRadius, 1.f, -1 );
    h_Circle /= cv::sum( h_Circle )[0];
    cv::filter2D( input, output, CV_32F, h_Circle );
}

void DoGaussianFilter( const int nFilterRadius, const double dSigma, img_t& input, img_t& output )
{
    int nDiameter = 2 * nFilterRadius + 1;
    cv::GaussianBlur( input, output, cv::Size( nDiameter, nDiameter ), dSigma );
}

void DoDilationFilter( const int nFilterRadius, img_t& input, img_t& output )
{
    int nDilationDiameter = 2 * nFilterRadius + 1;
    cv::Mat hDilationStructuringElement = cv::getStructuringElement( cv::MORPH_ELLIPSE, cv::Size( nDilationDiameter, nDilationDiameter ) );
    cv::dilate( input, output, hDilationStructuringElement );
}
#endif

// Collapse a vector of potentiall overlapping circles
// into a vector of non-overlapping circles
std::vector<Circle> CollapseCircles( const std::vector<Circle>& vInput )
{
	std::vector<Circle> vRet;

	for ( const Circle cInput : vInput )
	{
		bool bMatchFound = false;
		for ( Circle& cMatch : vRet )
		{
			// Compute distance from input to match
			float fDistX = cMatch.fX - cInput.fX;
			float fDistY = cMatch.fY - cInput.fY;
			float fDist2 = pow( fDistX, 2 ) + pow( fDistY, 2 );
			if ( fDist2 < pow( cInput.fR + cMatch.fR, 2 ) )
			{
				// Skip if they're very close
				if ( fDist2 < kEPS )
				{
					bMatchFound = true;
					continue;
				}

				// Compute union circle
				Circle cUnion { 0 };

				// Compute unit vector from input to match
				float fDist = sqrt( fDist2 );
				float nX = fDistX / fDist;
				float nY = fDistY / fDist;

				// Find furthest points on both circles
				float x0 = cInput.fX - nX * cInput.fR;
				float y0 = cInput.fY - nY * cInput.fR;
				float x1 = cMatch.fX + nX * cMatch.fR;
				float y1 = cMatch.fY + nY * cMatch.fR;

				// The distance between these points is the diameter of the union circle
				float fUnionDiameter = sqrt( pow( x1 - x0, 2 ) + pow( y1 - y0, 2 ) );
				cUnion.fR = fUnionDiameter / 2;

				// And the center is the midpoint between the furthest points
				cUnion.fX = ( x0 + x1 ) / 2;
				cUnion.fY = ( y0 + y1 ) / 2;

				// Replace this circle with the union circle
				cMatch = cUnion;
				bMatchFound = true;
			}
		}

		// If no match found, store this one
		if ( !bMatchFound )
		{
			vRet.push_back( cInput );
		}
	}

	// Run it again if we are still collapsing
	// This recursion will break when there are
	// no more circles to collapse
	if ( vInput.size() != vRet.size() )
		vRet = CollapseCircles( vRet );

	return vRet;
}

// Host version of FindStarsInImage
#if !SH_CUDA
std::vector<Circle> FindStarsInImage( float fStarRadius, img_t& dBoolImg )
{
    // We need a contiguous image of bytes (which we'll be treating as bools)
    if ( dBoolImg.type() != CV_8U || dBoolImg.empty() || dBoolImg.isContinuous() == false )
        throw std::runtime_error( "Error: Stars must be found in boolean images!" );

    // Find non-zero pixels in image, create circles
    std::vector<Circle> vRet;
    int x( 0 ), y( 0 );
#pragma omp parallel for shared(vRet, dBoolImg) private (x, y)
    for ( y = 0; y < dBoolImg.rows; y++ )
    {
        for ( x = 0; x < dBoolImg.cols; x++ )
        {
            uint8_t val = dBoolImg.at<uint8_t>( y, x );
            if ( val )
            {
#pragma omp critical
                vRet.push_back( { (float) x, (float) y, fStarRadius } );
            }
        }
    }

    // Collapse star images and return
    return CollapseCircles( vRet );
}
#endif
