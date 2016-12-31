#pragma once

#include <opencv2/opencv.hpp>

#include <memory>

#include "Util.h"

class ImageSource
{
public:
    enum class Status{
        WAIT,
        READY,
        DONE
    };

	virtual Status GetNextImage( img_t * pImg ) = 0;
    virtual void Initialize() {}
    virtual void Finalize() {}

	using Ptr = std::unique_ptr<ImageSource>;

	virtual ~ImageSource() {}
};

class ImageProcessor
{
public:
	virtual ~ImageProcessor() {}

	virtual bool HandleImage( img_t img ) = 0;	// Take an image as input

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
