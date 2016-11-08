#include "StarFinder.h"

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

struct IsNonzero
{
	IsNonzero() {}

	__host__ __device__
		bool operator()( const thrust::tuple<Byte, int> tup )
	{
		return thrust::get<0>( tup ) != 0;
	}
};

struct PixelIdxToLocation
{
	int m_nStep;
	PixelIdxToLocation( int nStep ) : m_nStep( nStep ) {}

	__host__ __device__
	thrust::pair<int, int> operator()( const int idx )
	{
		int x = idx % m_nStep;
		int y = idx / m_nStep;
		return thrust::make_pair( x, y );
	}
};

std::vector<std::pair<int, int>> FindStarsInImage( cv::cuda::GpuMat& dBoolImg )
{
	if ( dBoolImg.type() != CV_8U || dBoolImg.empty() || dBoolImg.isContinuous() == false )
		throw std::runtime_error( "Error: Stars must be found in boolean images!" );
	
	// Construct a device vector that we can iterate over from the mat's data
	using BytePtr = thrust::device_ptr<Byte>;

	using IdxVec = thrust::device_vector<int>;
	IdxVec dvIndices;
	
	// Create iterator to gives us pixel index (1-D)
	using CountIter = thrust::counting_iterator<int>;
	CountIter itCountBegin( 0 );
	CountIter itCountEnd( dBoolImg.size().area() );

	// Create an iterator that zips the pixel values with their 1-D index
	using PixelAndIdxIter = thrust::zip_iterator <thrust::tuple<BytePtr, CountIter>>;
	PixelAndIdxIter itPixAndIdxBegin = thrust::make_zip_iterator( thrust::make_tuple( BytePtr( (Byte *)dBoolImg.datastart ), itCountBegin ) );
	PixelAndIdxIter itPixAndIdxEnd = thrust::make_zip_iterator( thrust::make_tuple( BytePtr( (Byte *) dBoolImg.dataend ), itCountEnd ) );

	// Count the number of non-zero pixels (we need this so we can appropriately size dest vector)
	size_t count = thrust::count_if( itPixAndIdxBegin, itPixAndIdxEnd, IsNonzero() );
	dvIndices.resize( count );

	// Copy 1-D indices for non-zero pixels
	thrust::copy_if( itPixAndIdxBegin, itPixAndIdxEnd, thrust::make_zip_iterator( thrust::make_tuple( thrust::discard_iterator<>(), dvIndices.begin() ) ), IsNonzero() );

	// Transform this range into 2D coordinates
	using CoordVec = thrust::device_vector<thrust::pair<int, int>>;
	CoordVec dvNonzerPixelLocations( count );
	thrust::transform( dvIndices.begin(), dvIndices.end(), dvNonzerPixelLocations.begin(), PixelIdxToLocation( dBoolImg.step ) );

	cudaDeviceSynchronize();

	// Download to host and return
	std::vector < thrust::pair<int, int>> vRet( dvNonzerPixelLocations.size() );
	thrust::copy( dvNonzerPixelLocations.begin(), dvNonzerPixelLocations.end(), vRet.begin() );
	std::vector<std::pair<int, int>> vRet2;
	for ( thrust::pair<int, int>& p : vRet )
		vRet2.emplace_back( p.first, p.second );
	return vRet2;
}