#pragma once

// Weird thing I use to allow function pointers to std::function objects
#include <functional>
template <const size_t _UniqueId, typename _Res, typename... _ArgTypes>
struct fun_ptr_helper
{
public:
	typedef std::function<_Res( _ArgTypes... )> function_type;

	static void bind( function_type&& f )
	{
		instance().fn_.swap( f );
	}

	static void bind( const function_type& f )
	{
		instance().fn_ = f;
	}

	static _Res invoke( _ArgTypes... args )
	{
		return instance().fn_( args... );
	}

	typedef decltype( &fun_ptr_helper::invoke ) pointer_type;
	static pointer_type ptr()
	{
		return &invoke;
	}

private:
	static fun_ptr_helper& instance()
	{
		static fun_ptr_helper inst_;
		return inst_;
	}

	fun_ptr_helper() {}

	function_type fn_;
};

template <const size_t _UniqueId, typename _Res, typename... _ArgTypes>
typename fun_ptr_helper<_UniqueId, _Res, _ArgTypes...>::pointer_type
get_fn_ptr( const std::function<_Res( _ArgTypes... )>& f )
{
	fun_ptr_helper<_UniqueId, _Res, _ArgTypes...>::bind( f );
	return fun_ptr_helper<_UniqueId, _Res, _ArgTypes...>::ptr();
}

template<typename T>
std::function<typename std::enable_if<std::is_function<T>::value, T>::type>
make_function( T *t )
{
	return { t };
}

// Useful typedefs
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#if SH_CUDA
using img_t = cv::cuda::GpuMat;
#else
using img_t = cv::Mat;
#endif

// We use a different cv namespace for CUDA
#if SH_CUDA
using cv::cuda::max;
using cv::cuda::exp;
using cv::cuda::threshold;
using cv::cuda::subtract;
using cv::cuda::cvtColor;
#else
using cv::max;
using cv::exp;
using cv::threshold;
using cv::subtract;
using cv::cvtColor;
#endif

// Open a raw image file, implemented in filereader.cpp
img_t Raw2Img( void * pData, size_t uNumBytes );
img_t Raw2Img( std::string strFileName );	

// Display image with opencv (define both cv and gpu)
void displayImage( std::string strWindowName, cv::Mat& img );
#if SH_CUDA
void displayImage( std::string strWindowName, cv::cuda::GpuMat& img );
#endif