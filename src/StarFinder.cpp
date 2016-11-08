#include "StarFinder.h"

#include <opencv2/opencv.hpp>

// Two functions I use to display images, defined below
void displayImage( std::string strWindowName, cv::Mat& img );
void displayImage( std::string strWindowName, cv::cuda::GpuMat& img );

StarFinder::StarFinder( std::string strOutSuffix ) :
	m_uNumImagesProcessed( 0 ),
	m_strOutputSuffix( strOutSuffix )
{}

// Find stars in image
// Draw little circles around them
// Write image to disk
bool StarFinder::HandleImage( cv::Mat img )
{
	if ( img.empty() )
		return false;

	// So we know what we're working with here
	if ( img.type() != CV_8UC3 )
		throw std::runtime_error( "Error: What kind of image is StarFinder workign with?!" );

	// Upload input to GPU
	cv::cuda::GpuMat dInput;
	dInput.upload( img );

	// Convert to greyscale float
	cv::cuda::GpuMat tmp;
	cv::cuda::cvtColor( dInput, tmp, CV_RGB2GRAY );
	tmp.convertTo( dInput, CV_32F, 1.f / 0xff );

	// Find stars

	// Use guassian filter to remove noise
	int m_nGaussianRadius = 10;
	int nDiameter = 2 * m_nGaussianRadius + 1;

	// Create linear circle filter ("tophat filter")
	cv::Mat h_Circle = cv::Mat::zeros( cv::Size( nDiameter, nDiameter ), CV_32F );
	cv::circle( h_Circle, cv::Size( m_nGaussianRadius, m_nGaussianRadius ), m_nGaussianRadius, 1.f, -1 );
	h_Circle /= cv::sum( h_Circle )[0];
	cv::Ptr<cv::cuda::Filter> m_pLinCircFilter = cv::cuda::createLinearFilter( CV_32F, CV_32F, h_Circle );

	// Create gaussian filter
	const cv::Size szFilterDiameter( nDiameter, nDiameter );
	const float m_fHWHM = 2.f;
	const double dSigma = m_fHWHM / ( ( sqrt( 2 * log( 2 ) ) ) );
	cv::Ptr<cv::cuda::Filter> m_pGaussFilter = cv::cuda::createGaussianFilter( CV_32F, CV_32F, szFilterDiameter, dSigma );

	// Apply gaussian filter to input to remove high frequency noise
	cv::cuda::GpuMat dGaussian;
	m_pGaussFilter->apply( dInput, dGaussian );
	dbgImg( "Gaussian", dGaussian );

	// Apply linear filter to input to magnify high frequency noise
	cv::cuda::GpuMat dLinear;
	m_pLinCircFilter->apply( dInput, dLinear );
	dbgImg( "Linear", dLinear );

	// Subtract linear filtered image from gaussian image to clean area around peak
	// Noisy areas around the peak will be negative, so threshold negative values to zero
	cv::cuda::GpuMat dGaussianPeak;
	cv::cuda::subtract( dGaussian, dLinear, dGaussianPeak );
	cv::cuda::threshold( dGaussianPeak, dGaussianPeak, 0, 1, cv::THRESH_TOZERO );
	dbgImg( "Peaks", dGaussianPeak );

	// Create a thresholded image where the lowest pixel value is m_fIntensityThreshold
	float m_fIntensityThreshold = 0.05f;
	cv::cuda::GpuMat dThreshold( dGaussianPeak.size(), CV_32F, cv::Scalar( m_fIntensityThreshold ) );
	cv::cuda::max( dGaussianPeak, dThreshold, dThreshold );
	dbgImg( "Threshold", dThreshold );

	// Create the dilation filter
	int m_nDilationRadius = 5;
	int nDilationDiameter = 2 * m_nDilationRadius + 1;
	cv::Mat hDilationStructuringElement = cv::getStructuringElement( cv::MORPH_ELLIPSE, cv::Size( nDilationDiameter, nDilationDiameter ) );
	cv::Ptr<cv::cuda::Filter> m_pDilation = cv::cuda::createMorphologyFilter( cv::MORPH_DILATE, CV_32F, hDilationStructuringElement );

	// Create the dilated image (initialize its pixels to m_fIntensityThreshold)
	cv::cuda::GpuMat dDilated( dGaussianPeak.size(), CV_32F, cv::Scalar( m_fIntensityThreshold ) );
	m_pDilation->apply( dThreshold, dDilated );
	dbgImg( "Dilated", dDilated );

	// Subtract the dilated image from the gaussian peak image
	// What this leaves us with is an image where the brightest
	// gaussian peak pixels are zero and all others are negative
	cv::cuda::GpuMat dLocalMax;
	cv::cuda::subtract( dGaussianPeak, dDilated, dLocalMax );

	// Exponentiating that image makes those zero pixels 1, and the
	// negative pixels some low number; threshold to drop them
	cv::cuda::GpuMat dStarImage;
	const double kEps = .001;
	cv::cuda::exp( dLocalMax, dLocalMax );
	cv::cuda::threshold( dLocalMax, dStarImage, 1 - kEps, 1 + kEps, cv::THRESH_BINARY );
	dbgImg( "StarImage", dStarImage );

	// This star image is now a boolean image - convert it to bytes (TODO you should add some noise)
	cv::cuda::GpuMat dContigStarImg = cv::cuda::createContinuous( dStarImage.size(), CV_8U );
	dStarImage.convertTo( dContigStarImg, CV_8U, 0xff );
	dbgImg( "BoolImage", dContigStarImg );

	// Use thrust to find stars in pixel coordinates
	std::vector<std::pair<int, int>> vStarLocations = FindStarsInImage( dContigStarImg );

	// Create copy of original input and draw circles where stars were found
	cv::Mat matHightlight = img;
	const int nHighlightRadius = 10, nHighlightThickness = 1;
	const cv::Scalar sHighlightColor( 0xde, 0xad, 0 );
	for ( const std::pair<int, int> ptStar : vStarLocations )
	{
		// TODO should I be checking if we go too close to the edge?
		cv::circle( matHightlight, cv::Point( ptStar.first, ptStar.second ), nHighlightRadius, sHighlightColor, nHighlightThickness );
	}
	dbgImg( "Highlight", matHightlight );

	// Print highlighted image to disk
	std::string strOutputFileName = m_strOutputSuffix + std::to_string( m_uNumImagesProcessed++ ) + ".png";
	cv::imwrite( strOutputFileName, matHightlight );

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
	cv::destroyWindow( img );
}
