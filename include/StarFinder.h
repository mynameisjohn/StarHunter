#include "Engine.h"

#include <vector>
#include <utility>

// Basic image processing class that finds
// stars, determines their centroid, and
// dumps out an image where the stars found
// and their centroid are highlighted
class StarFinder : public ImageProcessor
{
	// Our "work buffer" of images to process
	std::list<cv::Mat> m_liMatsToProcess;

	// The files we'll write to disk will have
	// this suffix followed by their index
	size_t m_uNumImagesProcessed;
	std::string m_strOutputSuffix;

public:
	// TODO work out some algorithm parameters,
	// it's all hardcoded nonsense right now
	StarFinder( std::string strOutSuffix );

	bool HandleImage( cv::Mat img ) override;
};

// Forward declare cv::cuda::GpuMat
namespace cv
{
	namespace cuda
	{
		class GpuMat;
	}
}

// Takes in boolean star image and returns a vector of locations in pixels
// Uses thrust code, defined in StarFinder.cu
std::vector<std::pair<int, int>> FindStarsInImage( cv::cuda::GpuMat& dBoolImg );
