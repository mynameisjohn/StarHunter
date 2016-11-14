#include "StarFinder.h"

// I doubt I'm using all of these...
#include <thrust/device_ptr.h>
#include <thrust/device_vector.h>
#include <thrust/copy.h>
#include <thrust/iterator/discard_iterator.h>
#include <thrust/iterator/counting_iterator.h>
#include <thrust/count.h>
#include <thrust/transform.h>
#include <thrust/iterator/transform_iterator.h>
#include <thrust/execution_policy.h>
#include <thrust/iterator/zip_iterator.h>

#include <opencv2/cudaarithm.hpp>

#include <stdint.h>
#include <iostream>

using Byte = uint8_t;

// Look at zipped pixel and index, determine if pixel is nonzero
struct IsNonzero
{
	IsNonzero() {}

	__host__ __device__
		bool operator()( const thrust::tuple<Byte, int> tup )
	{
		return thrust::get<0>( tup ) != 0;
	}
};

// Convert 1-d pixel to star circle with radius
struct PixelToStarCircle
{
	// We need the image step to compute this
	float m_fRadius;
	int m_nStep;
	PixelToStarCircle( float fRadius, int nStep ) : m_fRadius( fRadius ), m_nStep( nStep ) {}

	__host__ __device__
	Circle operator()( const int idx )
	{
		int x = idx % m_nStep;
		int y = idx / m_nStep;
		return { (float) x, (float) y, m_fRadius };
	}
};

// Find non-zero pixel locations and return a vector of their pixel coordinates
std::vector<Circle> FindStarsInImage( float fStarRadius, cv::cuda::GpuMat& dBoolImg )
{
	// We need a contiguous image of bytes (which we'll be treating as bools)
	if ( dBoolImg.type() != CV_8U || dBoolImg.empty() || dBoolImg.isContinuous() == false )
		throw std::runtime_error( "Error: Stars must be found in boolean images!" );

	// Construct a device vector that we can iterate over from the mat's data
	using BytePtr = thrust::device_ptr<Byte>;

	// Create iterator to gives us pixel index (1-D)
	using CountIter = thrust::counting_iterator<int>;
	CountIter itCountBegin( 0 );
	CountIter itCountEnd( dBoolImg.size().area() );

	// Create an iterator that zips the pixel values with their 1-D index
	using PixelAndIdxIter = thrust::zip_iterator <thrust::tuple<BytePtr, CountIter>>;
	PixelAndIdxIter itPixAndIdxBegin = thrust::make_zip_iterator( thrust::make_tuple( BytePtr( (Byte *)dBoolImg.datastart ), itCountBegin ) );
	PixelAndIdxIter itPixAndIdxEnd = thrust::make_zip_iterator( thrust::make_tuple( BytePtr( (Byte *) dBoolImg.dataend ), itCountEnd ) );

	// Count the number of non-zero pixels (we need this so we can appropriately size dest vector)
	using IdxVec = thrust::device_vector<int>;
	IdxVec dvIndices;
	size_t count = thrust::count_if( itPixAndIdxBegin, itPixAndIdxEnd, IsNonzero() );
	dvIndices.resize( count );

	// Copy 1-D indices for non-zero pixels, discard the pixel values
	thrust::copy_if( itPixAndIdxBegin, itPixAndIdxEnd, thrust::make_zip_iterator( thrust::make_tuple( thrust::discard_iterator<>(), dvIndices.begin() ) ), IsNonzero() );

	// Transform this range into star circles
	using CircleVec = thrust::device_vector<Circle>;
	CircleVec dvStarCircles( count );
	thrust::transform( dvIndices.begin(), dvIndices.end(), dvStarCircles.begin(), PixelToStarCircle( fStarRadius, dBoolImg.step ) );

	// Don't know if this is necessary
	cudaDeviceSynchronize();

	// Download to host
	std::vector<Circle> hvStarCircles( dvStarCircles.size() );
	thrust::copy( dvStarCircles.begin(), dvStarCircles.end(), hvStarCircles.begin() );

	// Collapse
	std::vector<Circle> vStarPos_Collapsed = CollapseCircles( hvStarCircles );

	// Return collapsed star positions
	return vStarPos_Collapsed;
}
