#include "Engine.h"

#include <vector>
#include <utility>

#include <opencv2/opencv.hpp>

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

// Takes in boolean star image and returns a vector of locations in pixels
// Uses thrust code, defined in StarFinder.cu
std::vector<std::pair<int, int>> FindStarsInImage( cv::cuda::GpuMat& dBoolImg );
