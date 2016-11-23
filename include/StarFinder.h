#include "Engine.h"

#include <memory>
#include <vector>
#include <utility>

#include <opencv2/opencv.hpp>

// Useful typedefs
#if SH_CUDA
using img_t = cv::cuda::GpuMat;
#else
using img_t = cv::Mat;
#endif

// Circle struct - made my
// own so it can be constructed
// in host and device code
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
	img_t m_imgInput;
	img_t m_imgGaussian;
	img_t m_imgTopHat;
	img_t m_imgPeak;
	img_t m_imgThreshold;
	img_t m_imgDilated;
	img_t m_imgLocalMax;
	img_t m_imgStars;
	img_t m_imgBoolean;
	img_t m_imgTmp;

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
class StarFinder_Drift : public StarFinder
{
	// The drift will be averaged when requested
	int m_nImagesProcessed;
	float m_fDriftX_Cumulative;
	float m_fDriftY_Cumulative;

	// The last set of stars we found
	// We'll be looking for their match
	std::vector<Circle> m_vLastCircles;
public:
	StarFinder_Drift();
	bool HandleImage( cv::Mat img ) override;
	bool GetDrift( float * pDriftX, float * pDriftY ) const;
	bool GetDriftN( float * pDriftX, float * pDriftY ) const;
};

class FileReader_WithOfs;
class StarFinder_ImgOffset : public StarFinder_Drift
{
    FileReader_WithOfs * m_pFileReader;

public:
    StarFinder_ImgOffset(FileReader_WithOfs * pFileReader);
    bool HandleImage( cv::Mat img ) override;
};

// Finds overlapping circles and combines them
std::vector<Circle> CollapseCircles( const std::vector<Circle>& vInput );

// Takes in boolean star image and returns a vector of stars as circles
std::vector<Circle> FindStarsInImage( float fStarRadius, img_t& dBoolImg );
