#include "StarFinder.h"
#include "FnPtrHelper.h"

#include <opencv2/opencv.hpp>
// Two functions I use to display images, defined below
void displayImage( std::string strWindowName, cv::Mat& img );
void displayImage( std::string strWindowName, cv::cuda::GpuMat& img );

const double kEPS = .001;

StarFinder::StarFinder() :
	// These are some good defaults
	m_nGaussianRadius( 10 ),
	m_nDilationRadius( 5 ),
	m_fHWHM( 2.5f ),
	m_fIntensityThreshold( 0.25f )
{}

bool StarFinder::findStars( cv::Mat& img )
{
	if ( img.empty() )
		return false;

	// So we know what we're working with here
	if ( img.type() != CV_8UC3 )
		throw std::runtime_error( "Error: What kind of image is StarFinder working with?!" );

	// Initialize if we haven't yet
	if ( m_dInputImg.empty() )
	{
		// Preallocate the GPU mats needed during computation
		m_dInputImg = cv::cuda::GpuMat( img.size(), img.type() );
		m_dGaussianImg = cv::cuda::GpuMat( img.size(), CV_32F );
		m_dTopHatImg = cv::cuda::GpuMat( img.size(), CV_32F );
		m_dPeakImg = cv::cuda::GpuMat( img.size(), CV_32F );
		m_dThresholdImg = cv::cuda::GpuMat( img.size(), CV_32F );
		m_dDilatedImg = cv::cuda::GpuMat( img.size(), CV_32F );
		m_dLocalMaxImg = cv::cuda::GpuMat( img.size(), CV_32F );
		m_dStarImg = cv::cuda::GpuMat( img.size(), CV_32F );
		m_dBoolImg = cv::cuda::createContinuous( img.size(), CV_8U );
	}

	// Upload input to GPU
	m_dInputImg.upload( img );

	// Convert to greyscale float (using temp image)
	cv::cuda::cvtColor( m_dInputImg, m_dTmpImg, CV_RGB2GRAY );
	m_dTmpImg.convertTo( m_dInputImg, CV_32F, 1.f / 0xff );

	// Use guassian filter to remove noise
	int nDiameter = 2 * m_nGaussianRadius + 1;

	// Create linear circle filter ("tophat filter")
	cv::Mat h_Circle = cv::Mat::zeros( cv::Size( nDiameter, nDiameter ), CV_32F );
	cv::circle( h_Circle, cv::Size( m_nGaussianRadius, m_nGaussianRadius ), m_nGaussianRadius, 1.f, -1 );
	h_Circle /= cv::sum( h_Circle )[0];
	cv::Ptr<cv::cuda::Filter> m_pLinCircFilter = cv::cuda::createLinearFilter( CV_32F, CV_32F, h_Circle );

	// Create gaussian filter
	const cv::Size szFilterDiameter( nDiameter, nDiameter );
	const double dSigma = m_fHWHM / ( ( sqrt( 2 * log( 2 ) ) ) );
	cv::Ptr<cv::cuda::Filter> m_pGaussFilter = cv::cuda::createGaussianFilter( CV_32F, CV_32F, szFilterDiameter, dSigma );

	// Create the dilation kernel
	int nDilationDiameter = 2 * m_nDilationRadius + 1;
	cv::Mat hDilationStructuringElement = cv::getStructuringElement( cv::MORPH_ELLIPSE, cv::Size( nDilationDiameter, nDilationDiameter ) );
	cv::Ptr<cv::cuda::Filter> m_pDilation = cv::cuda::createMorphologyFilter( cv::MORPH_DILATE, CV_32F, hDilationStructuringElement );

	// Apply gaussian filter to input to remove high frequency noise
	m_pGaussFilter->apply( m_dInputImg, m_dGaussianImg );

	// Apply linear filter to input to magnify high frequency noise
	m_pLinCircFilter->apply( m_dInputImg, m_dTopHatImg );

	// Subtract linear filtered image from gaussian image to clean area around peak
	// Noisy areas around the peak will be negative, so threshold negative values to zero
	cv::cuda::subtract( m_dGaussianImg, m_dTopHatImg, m_dPeakImg);
	cv::cuda::threshold( m_dPeakImg, m_dPeakImg, 0, 1, cv::THRESH_TOZERO );

	// Create a thresholded image where the lowest pixel value is m_fIntensityThreshold
	m_dThresholdImg.setTo( cv::Scalar( m_fIntensityThreshold ) );
	cv::cuda::max( m_dPeakImg, m_dThresholdImg, m_dThresholdImg );

	// Create the dilated image (initialize its pixels to m_fIntensityThreshold)
	m_dDilatedImg.setTo( cv::Scalar( m_fIntensityThreshold ) );
	m_pDilation->apply( m_dThresholdImg, m_dDilatedImg );

	// Subtract the dilated image from the gaussian peak image
	// What this leaves us with is an image where the brightest
	// gaussian peak pixels are zero and all others are negative
	cv::cuda::subtract( m_dPeakImg, m_dDilatedImg, m_dLocalMaxImg );

	// Exponentiating that image makes those zero pixels 1, and the
	// negative pixels some low number; threshold to drop them
	cv::cuda::exp( m_dLocalMaxImg, m_dLocalMaxImg );
	cv::cuda::threshold( m_dLocalMaxImg, m_dStarImg, 1 - kEPS, 1 + kEPS, cv::THRESH_BINARY );

	// This star image is now a boolean image - convert it to bytes (TODO you should add some noise)
	m_dStarImg.convertTo( m_dBoolImg, CV_8U, 0xff );

	return true;
}

// Just find the stars
bool StarFinder::HandleImage( cv::Mat img )
{
	return findStars( img );
}

StarFinder_UI::StarFinder_UI() :
	StarFinder()
{}

bool StarFinder_UI::HandleImage( cv::Mat img )
{
	// Call this once to test input and init images
	if ( !findStars( img ) )
		return false;

	// Create window, trackbars
	////////////////////////////////////////////////////
	// Window name
	const std::string strWindowName = "Star Finder";
	cv::namedWindow( strWindowName, cv::WINDOW_AUTOSIZE );

	// Trackbar parameter Names
	std::string strGaussRadiusTBName = "Gaussian Radius";
	std::string strHWHMTBName = "Half-Width at Half-Maximum ";
	std::string strDilationRadiusTBName = "Dilation Radius";
	std::string strIntensityThreshTBName = "Intensity Threshold";

	// We need pointers to these ints
	std::map<std::string, int> mapParamValues = {
		{ strGaussRadiusTBName, m_nGaussianRadius },	// These are the
		{ strHWHMTBName, (int)m_fHWHM },			// default values
		{ strDilationRadiusTBName, m_nDilationRadius },// specified in the
		{ strIntensityThreshTBName, (int) ( 100.f * m_fIntensityThreshold ) } // PLuTARC_testbed
	};

	// Scale values by resolution
	const float fTrackBarRes = 1000.f;
	for ( auto& itNameToParam : mapParamValues )
		itNameToParam.second *= fTrackBarRes;

	// Trackbar Callback
	// Trackbar callback, implemented below
	std::function<void( int, void * )> trackBarCallback = [&, this]( int pos, void * priv )
	{
		// Set parameters
		m_nGaussianRadius = mapParamValues[strGaussRadiusTBName] / fTrackBarRes;
		m_fHWHM = mapParamValues[strHWHMTBName] / fTrackBarRes;
		m_nDilationRadius = mapParamValues[strDilationRadiusTBName] / fTrackBarRes;
		m_fIntensityThreshold = mapParamValues[strIntensityThreshTBName] / ( 100.f*fTrackBarRes );

		// Find stars (results are in member images)
		if ( findStars( img ) == false )
			throw std::runtime_error( "Error! Why did findStars return false?" );

		// Use thrust to find stars in pixel coordinates
		const float fStarRadius = 10.f;
		std::vector<Circle> vStarLocations = FindStarsInImage( fStarRadius, m_dBoolImg );

		// Create copy of original input and draw circles where stars were found
		cv::Mat hHightlight = img.clone();
		const int nHighlightThickness = 1;
		const cv::Scalar sHighlightColor( 0xde, 0xad, 0 );
		
		for ( const Circle ptStar : vStarLocations )
		{
			// TODO should I be checking if we go too close to the edge?
			cv::circle( hHightlight, cv::Point( ptStar.fX, ptStar.fY ), ptStar.fR, sHighlightColor, nHighlightThickness );
		}

		// Show the image with circles
		cv::resizeWindow( strWindowName, hHightlight.size().width, hHightlight.size().height );
		cv::imshow( strWindowName, hHightlight );
	};

	// Create trackbars
	auto createTrackBar = [&mapParamValues, strWindowName, &trackBarCallback] ( std::string tbName, int maxVal ) {
		auto it = mapParamValues.find( tbName );
		if ( it != mapParamValues.end() )
		{
			cv::createTrackbar( tbName, strWindowName, &mapParamValues[tbName], maxVal, get_fn_ptr<0>( trackBarCallback ) );
		}
	};
	createTrackBar( strGaussRadiusTBName, 15 * fTrackBarRes );
	createTrackBar( strHWHMTBName, 15 * fTrackBarRes );
	createTrackBar( strDilationRadiusTBName, 15 * fTrackBarRes );
	createTrackBar( strIntensityThreshTBName, 15 * fTrackBarRes );

	// Call the callback once to initialize the window
	trackBarCallback( 0, nullptr );

	// Wait while user sets things until they press a key (any key?)
	cv::waitKey();

	return true;
}

StarFinder_OptFlow::StarFinder_OptFlow() :
	StarFinder(),
	m_fDriftX( 0 ),
	m_fDriftY( 0 )
{}

bool StarFinder_OptFlow::HandleImage( cv::Mat img )
{
	if ( !findStars( img ) )
		return false;

	if ( m_vLastCircles.empty() )
	{
		// Store if not yet created
		m_vLastCircles = FindStarsInImage( 10, m_dBoolImg );
	}
	else
	{
		// Use thrust to find stars in pixel coordinates
		std::vector<Circle> vStarLocations = FindStarsInImage( 10, m_dBoolImg );

		// If these are sized different, we have problems
		if ( vStarLocations.size() != m_vLastCircles.size() )
			throw std::runtime_error( "We lost some stars" );

		for ( Circle c1 : m_vLastCircles )
		{
			bool bMatchFound = false;
			for ( Circle c2 : vStarLocations )
			{
				float fDistX = c1.fX - c2.fX;
				float fDistY = c1.fY - c2.fY;
				float fDist2 = pow( fDistX, 2 ) + pow( fDistY, 2 );
				if ( fDist2 < pow( c1.fR + c2.fR, 2 ) )
				{
					if ( bMatchFound )
						throw std::runtime_error( "Error: Why were there two matches?" );
					bMatchFound = true;

					// We're adding the average drift across all stars
					m_fDriftX += fDistX / (float) vStarLocations.size();
					m_fDriftY += fDistY / (float) vStarLocations.size();
				}
			}

			if ( bMatchFound == false )
				throw std::runtime_error( "Error: No matches found!" );
		}

		// Update cached positions
		m_vLastCircles = std::move( vStarLocations );
	}

	return true;
}

void displayImage( std::string strWindowName, cv::cuda::GpuMat& img )
{
	cv::namedWindow( strWindowName, CV_WINDOW_OPENGL );
	cv::imshow( strWindowName, img );
	cv::waitKey();
	cv::destroyWindow( strWindowName );
}

void displayImage( std::string strWindowName, cv::Mat& img )
{
	cv::namedWindow( strWindowName, CV_WINDOW_FREERATIO );
	cv::imshow( strWindowName, img );
	cv::waitKey();
	cv::destroyWindow( strWindowName );
}

// Collapse a vector of potentiall overlapping circles
// into a vector of non-overlapping circles
std::vector<Circle> CollapseCircles( const std::vector<Circle>& vInput )
{
	std::vector<Circle> vRet;

	for ( const Circle c : vInput )
	{
		bool bMatchFound = false;
		for ( Circle& cMatch : vRet )
		{
			float fDistX = cMatch.fX - c.fX;
			float fDistY = cMatch.fY - c.fY;
			float fDist2 = pow( fDistX, 2 ) + pow( fDistY, 2 );
			if ( fDist2 < pow( c.fR + cMatch.fR, 2 ) )
			{
				// Skip if they're very close
				if ( fDist2 < kEPS )
				{
					bMatchFound = true;
					continue;
				}

				// Compute union circle
				Circle cUnion { 0 };

				// Compute unit vector between centers
				float fDist = sqrt( fDist2 );
				float nX = fDistX / fDist;
				float nY = fDistY / fDist;

				// Find furthest points on both circles
				float x0 = c.fX + nX * c.fR;
				float y0 = c.fY + nY * c.fR;
				float x1 = cMatch.fX - nX * cMatch.fR;
				float y1 = cMatch.fY - nY * cMatch.fR;

				// The distance between these points is
				// the diameter of the union circle
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
			vRet.push_back( c );
		}
	}

	return vRet;
}