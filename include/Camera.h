#pragma once

#if SH_CAMERA

#include "Engine.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

#if SH_USE_EDSDK
#include "EDSDK.h"

#include <mutex>
#include <thread>
#include <list>
#include <memory>
#include <atomic>
#include <chrono>

#include "Command.h"

#else
#include <gphoto2/gphoto2.h>
#endif

class SHCamera : public ImageSource
#if SH_USE_EDSDK
	, public DownloadEvfCommand::Receiver
	, public DownloadCommand::Receiver
#endif
{
public:
	enum class Mode
	{
		Off,		// Nothing
		Streaming,	// Viewfinder input
		Capturing	// Actually taking pictures, writing to disk
	};

private:
	// A thread is spawned to capture images
	// and store them in this list. The list
	// is cleared whenever modes are switched
    std::thread m_thCapture;
    std::mutex m_muCapture;

	// These images are returned via GetNextImage
	// if the mode is streaming - if the mode is
	// Capturing, WAIT is always returned until
	// the capturing is done (img limit hit)
    std::list<img_t> m_liCapturedImages;

	// Camera mode, can be accessed
	// and modified from threads so
	// protected by a mutex
	std::mutex m_muCamMode;
	Mode m_eMode;

	// The capture mode will write images to disk and
	// return the WAIT status from GetNextImage until
	// a predetermined number of images are captured
	int m_nImageCaptureLimit;
	int m_nImagesCaptured;
	int m_nShutterDuration;

	// The captured images written to disk
	// will be named with this prefix
	std::string m_strImgCapturePrefix;
#if SH_USE_EDSDK
	// A pointer to the camera model object
	// This owns the EDSDK reference to the camera
	// and fields the get/setProperty callbacks
	std::unique_ptr<CameraModel> m_pCamModel;

	// We use a thread safe command queue
	// to control the camera
	CommandQueue m_CMDQueue;
	
	// The streaming code takes an average
	// of the incoming frames - the images
	// being averaged are stored here
	std::list<cv::Mat> m_liImageStack;



#else
    GPContext * m_pGPContext;
    Camera * m_pGPCamera;
#endif

public:
	SHCamera( std::string strNamePrefix, int nImagesToCapture, int nShutterDuration );
    ~SHCamera();

	// Getter and setter for camera mode
	void SetMode( const Mode m );
	Mode GetMode();

	// ImageSource overrides
	ImageSource::Status GetNextImage( img_t * pImg ) override;
    void Initialize() override;
    void Finalize() override;

#if SH_USE_EDSDK
	// EVF receiver override, posts to main thread
	bool handleEvfImage() override;

	// Captured image handler, downloads to disk
	bool handleCapturedImage( EdsDirectoryItemRef dirItem ) override;
#endif

private:

	// The thread function, which starts when the
	// mode transitions from off to something else,
	// and exits when the mode transitions to off
    void threadProc();

#if SH_USE_EDSDK
    EdsError EDSCALLBACK handleObjectEvent_impl( EdsUInt32			inEvent,
                                     EdsBaseRef			inRef,
                                     EdsVoid *			inContext );
    EdsError EDSCALLBACK handleStateEvent_impl(
        EdsUInt32			inEvent,
        EdsUInt32			inParam,
        EdsVoid *			inContext
    );

	EdsError EDSCALLBACK handlePropertyEvent_impl(
		EdsUInt32			inEvent,
		EdsUInt32			inPropertyID,
		EdsUInt32			inParam,
		EdsVoid *			inContext
	);

    static EdsError EDSCALLBACK handleObjectEvent(
        EdsUInt32			inEvent,
        EdsBaseRef			inRef,
        EdsVoid *			inContext
    );

    static EdsError EDSCALLBACK  handleStateEvent(
        EdsUInt32			inEvent,
        EdsUInt32			inParam,
        EdsVoid *			inContext
    );	
	
	static EdsError EDSCALLBACK  handlePropertyEvent(
		EdsUInt32			inEvent,
		EdsUInt32			inPropertyID,
		EdsUInt32			inParam,
		EdsVoid *			inContext
	);
#endif
};
#endif
