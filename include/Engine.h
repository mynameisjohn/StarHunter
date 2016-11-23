#pragma once

#include <opencv2/opencv.hpp>

#include <memory>

class ImageSource
{
public:
	virtual bool HasImages() const = 0;		// Are there any images left?
	virtual cv::Mat GetNextImage() = 0;	// Get the next image
    virtual void Initialize() {}
    virtual void Finalize() {}

	using Ptr = std::unique_ptr<ImageSource>;
};

class ImageProcessor
{
public:
	virtual bool HandleImage( cv::Mat img ) = 0;	// Take an image as input

    virtual void Initialize() {}
	virtual void Finalize() {}						

	using Ptr = std::unique_ptr<ImageProcessor>;
};

class Engine
{
	ImageSource::Ptr m_pImageSource;
	ImageProcessor::Ptr m_pImageProcessor;
public:
	Engine( ImageSource::Ptr&& pImgSrc, ImageProcessor::Ptr&& pImgProc );
	void Run();
};
