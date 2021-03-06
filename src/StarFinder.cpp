#include "StarFinder.h"
#include "FileReader.h"
#include "Util.h"

#if SH_CAMERA
#include "Camera.h"
#include <SDL.h>
#endif // SH_CAMERA

#if SH_TELESCOPE
#include "TelescopeComm.h"
#include <pyliaison.h>
#endif // SH_TELESCOPE

#if SH_CAMERA && SH_TELESCOPE
#include "ImageTextureWindow.h"
#include <pyliaison.h>
#endif

#include <opencv2/opencv.hpp>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

// Filtering functions, defined below
void DoGaussianFilter( const int nFilterRadius, const double dSigma, img_t& input, img_t& output );
void DoTophatFilter( const int nFilterRadius, img_t& input, img_t& output );
void DoDilationFilter( const int nFilterRadius, img_t& input, img_t& output );

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
	
	// Work with copy of original
	m_imgInput = img.clone();

	// I think I can just steal the reference
//	// Upload input to GPU for CUDA
//#if SH_CUDA
//    m_imgInput.upload( img );
//#else
//    m_imgInput = img.clone();
//#endif

	int nFilterRadius = std::min<int>( 15, ( .5f + m_fFilterRadius * m_imgInput.cols ) );
	int nDilationRadius = std::min<int>( 15, ( .5f + m_fDilationRadius * m_imgInput.cols ) );

	// Apply gaussian filter to input to remove high frequency noise
	const double dSigma = m_fHWHM / ( ( sqrt( 2 * log( 2 ) ) ) );
	DoGaussianFilter( nFilterRadius, dSigma, m_imgInput, m_imgGaussian );

	// Apply linear filter to input to magnify high frequency noise
	DoTophatFilter( nFilterRadius, m_imgInput, m_imgTopHat );

	// Subtract linear filtered image from gaussian image to clean area around peak
	// Noisy areas around the peak will be negative, so threshold negative values to zero
	::subtract( m_imgGaussian, m_imgTopHat, m_imgPeak );
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
				// We've come full circle...
				if ( bMatchFound )
					break;

				// Compute the drift between old and new
				float fDistX = cNew.fX - cOld.fX;
				float fDistY = cNew.fY - cOld.fY;
				float fDist2 = pow( fDistX, 2 ) + pow( fDistY, 2 );
				if ( fDist2 < pow( cOld.fR + cNew.fR, 2 ) )
				{
					// Divide this drift by the # of stars we have to average
					fDriftAvgX += fDistX / float( m_vLastCircles.size() );
					fDriftAvgY += fDistY / float( m_vLastCircles.size() );
					bMatchFound = true;
				}
			}
		}

		// Update cached positions, inc cumulative drift counter
		m_vLastCircles = vStarLocations;
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

StarFinder_ImgOffset::StarFinder_ImgOffset( FileReader_WithDrift * pFileReader ) :
	StarFinder_Drift(),
	m_pFileReader( pFileReader )
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
			float fDriftX( 0 ), fDriftY( 0 );
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

#if SH_CAMERA && SH_TELESCOPE

bool StarHunter::Run()
{
	try
	{
#if SH_USE_EDSDK
		// Create SDL window for Windows
		m_upTextureWindow.reset( new ImageTextureWindow( "StarHunter",
														 SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 400, 400,
														 SDL_WINDOW_SHOWN, 3, 0, true,
														 "../shaders/shader.vert", "../shaders/shader.frag", 0.7f ) );
		if ( m_upTextureWindow == nullptr )
			return false;
#endif
		// Init pyliaison
		pyl::initialize();

		// Declare slew cmd counter (set to limit 
		int nImagesTillSlewCMD( m_nImagesPerSlewCMD );

		// Detect, then calibrate, then track, then get out
		for ( m_eState = State::NONE; m_eState != State::DONE;)
		{
			// Variable declarations
			img_t img;
			float fDriftX( 0 ), fDriftY( 0 );
			int nSlewRateX( 0 ), nSlewRateY( 0 );
			ImageSource::Status eImgStat = ImageSource::Status::READY;

			// Pump input, allow exit
#if SH_USE_EDSDK
			SDL_Event e { 0 };
			while ( SDL_PollEvent( &e ) )
			{
				if ( e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_ESCAPE )
					m_eState = State::DONE;
			}
#endif

			switch ( m_eState )
			{
				// Initially set camera mode to streaming, set state to detect, and break
				case State::NONE:
					// Init camera and set to stream
					m_upCamera->Initialize();	
					m_upCamera->SetMode( SHCamera::Mode::Streaming );

					// Init telescope comm
					m_upTelescopeComm->Initialize();

					// We're detecting
					m_eState = State::DETECT;
					break;

					// During the detect phase, we call GetNextImage on the camera
					// and send it to the star finder. Eventually the sf will get
					// a drift value, at which point we start calibrating
				case State::DETECT:
					// Get an image from the camera
					eImgStat = m_upCamera->GetNextImage( &img );
					if ( eImgStat == ImageSource::Status::READY )
						m_upStarFinder->HandleImage( img );
					else
						break;

					std::cout << "Trying to detect stars in image..." << std::endl;

					// This will return true once there are things with drift detected
					if ( m_upStarFinder->GetDrift_Cumulative( &fDriftX, &fDriftY ) )
					{
						// Switch to calibration state
						std::cout << "Stars detected in input! moving on to calibration" << std::endl;
						m_eState = State::CALIBRATE;
						break;
					}
					break;

					// The calibration state will change the slew rate of the
					// telescope mount until the drift values go below some threshold
				case State::CALIBRATE:
					// Get an image from the camera
					eImgStat = m_upCamera->GetNextImage( &img );
					if ( eImgStat == ImageSource::Status::READY )
						m_upStarFinder->HandleImage( img );
					else
						break;

					// Get the most recent drift value (this shouldn't return false...)
					if ( m_upStarFinder->GetDrift_Prev( &fDriftX, &fDriftY ) )
					{
						std::cout << "Calibrating with drift value of " << fDriftX << ", " << fDriftX << std::endl;

						// Are we drifting up/down?
						bool bStableX = fabs( fDriftX ) < kEPS;
						bool bStableY = fabs( fDriftY ) < kEPS;

						// If these are both below the threshold, set state to track
						if ( bStableX && bStableY )
						{
							std::cout << "Calibration complete! Stars are now being tracked" << std::endl;
							m_eState = State::TRACK;
							m_upCamera->SetMode( SHCamera::Mode::Capturing );
							break;
						}

#if SH_TELESCOPE
						// Increment the slew CMD counter, send a command if we've hit it
						if ( nImagesTillSlewCMD++ % m_nImagesPerSlewCMD == 0 )
						{
							std::cout << "Sending slew rate command to mount: " << nSlewRateX << ", " << nSlewRateY << std::endl;

							// Add 1 in the direction of drift
							m_upTelescopeComm->GetSlewRate( &nSlewRateX, &nSlewRateY );
							if ( !bStableX )
								nSlewRateX += fDriftX > 0 ? 1 : -1;
							if ( !bStableY )
								nSlewRateY += fDriftY > 0 ? 1 : -1;

							// Reset counter
							nImagesTillSlewCMD = 0;

							// Set slew rate (noop if no difference)
							m_upTelescopeComm->SetSlewRate( nSlewRateX, nSlewRateY );
						}
#endif
					}
					break;

					// Not much for us to do in the tracking state, we are just
					// making the camera take images and storing them
				case State::TRACK:
					// This will return DONE when the camera is out of images
					if ( m_upCamera->GetNextImage( &img ) == ImageSource::Status::DONE )
					{
						m_eState = State::DONE;
						m_upCamera->SetMode( SHCamera::Mode::Off );
					}
					break;
				case State::DONE:
					m_upCamera->Finalize();
					break;
			}

#if SH_USE_EDSDK
			// Display the image if we have one
			if ( img.empty() == false )
				m_upTextureWindow->SetImage( img );
			m_upTextureWindow->Draw();
#endif
		}

		// Finalize pyl
		pyl::finalize();
	}
	catch ( std::runtime_error& e )
	{
		pyl::finalize();
		std::cout << e.what() << std::endl;
		return false;
	}

	return true;
}

StarHunter::StarHunter( int nImagesTillSlew, SHCamera * pCamera, TelescopeComm * pTelescopeComm, StarFinder_Drift * pStarFinder ) :
	m_nImagesPerSlewCMD( std::max( 1, nImagesTillSlew ) ),
	m_upCamera( pCamera ),
	m_upTelescopeComm( pTelescopeComm ),
	m_upStarFinder( pStarFinder )
{}

StarHunter::~StarHunter() {}

#endif // SH_CAMERA && SH_TELESCOPE

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
