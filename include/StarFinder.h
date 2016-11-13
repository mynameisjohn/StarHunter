#include "Engine.h"

#include <vector>
#include <utility>

#include <opencv2/opencv.hpp>


// Circle struct
struct Circle
{
	float fX;	// x pos
	float fY;	// y pos
	float fR;	// radius
};

// Base star finder class
// All it's for is calling findStars,
// which performs the signal processing
// and leaves m_dBoolImg with a byte
// image where nonzero pixels are stars
class StarFinder : public ImageProcessor
{
protected:
	// Processing params
	int m_nGaussianRadius;
	int m_nDilationRadius;
	float m_fHWHM;
	float m_fIntensityThreshold;

	// The images we use and their size
	cv::cuda::GpuMat m_dInputImg;
	cv::cuda::GpuMat m_dGaussianImg;
	cv::cuda::GpuMat m_dTopHatImg;
	cv::cuda::GpuMat m_dPeakImg;
	cv::cuda::GpuMat m_dThresholdImg;
	cv::cuda::GpuMat m_dDilatedImg;
	cv::cuda::GpuMat m_dLocalMaxImg;
	cv::cuda::GpuMat m_dStarImg;
	cv::cuda::GpuMat m_dBoolImg;
	cv::cuda::GpuMat m_dTmpImg;

	// Leaves bool image with star locations
	bool findStars( cv::Mat& img );

public:
	// TODO work out some algorithm parameters,
	// it's all hardcoded nonsense right now
	StarFinder();

	bool HandleImage( cv::Mat img ) override;
};

// UI implementation - pops opencv window
// with trackbars to control parameters and
// see results
class StarFinder_UI : public StarFinder
{
public:
	StarFinder_UI();
	bool HandleImage( cv::Mat img ) override;
};

// Implementation that computes the average
// drift in star positions across each
// handled image. 
class StarFinder_OptFlow : public StarFinder
{
	// The drift will be averaged when requested
	int m_nImagesProcessed;
	float m_fDriftX_Cumulative;
	float m_fDriftY_Cumulative;

	// The last set of stars we found
	// We'll be looking for their match
	std::vector<Circle> m_vLastCircles;
public:
	StarFinder_OptFlow();
	bool HandleImage( cv::Mat img ) override;
	bool GetDrift( float * pDriftX, float * pDriftY ) const;
};



// Takes in boolean star image and returns a vector of stars as circles
std::vector<Circle> FindStarsInImage( float fStarRadius, cv::cuda::GpuMat& dBoolImg );
std::vector<Circle> CollapseCircles( const std::vector<Circle>& vInput );
