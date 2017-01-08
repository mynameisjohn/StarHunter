#if SH_CAMERA

#include <libraw/libraw.h>
#include <chrono>

#include "Util.h"
#include "Camera.h"

#if SH_USE_EDSDK

#else
void checkErr( const int retVal, std::string strName )
{
    if ( retVal != GP_OK )
    {
        std::string strErrMsg = "Error: " + strName + " failed with error code " + std::to_string( retVal );
        throw std::runtime_error( strErrMsg );
    }
}
#endif

SHCamera::SHCamera( std::string strNamePrefix, int nImagesToCapture, int nShutterDuration ) :
	m_eMode( Mode::Off ),
	m_strImgCapturePrefix( strNamePrefix ),
	m_nImageCaptureLimit( nImagesToCapture ),
	m_nImagesCaptured( 0 ),
	m_nShutterDuration( nShutterDuration )
#ifndef SH_USE_EDSDK
	m_pGPContext( nullptr ),
	m_pGPCamera( nullptr )
#endif
{}

SHCamera::~SHCamera()
{
	SetMode( Mode::Off );
    Finalize();
}

ImageSource::Status SHCamera::GetNextImage( img_t * pImg )
{
    {
		std::lock_guard<std::mutex> lg( m_muCapture );
		//std::cout << m_liCapturedImages.size() << std::endl;
		if ( !m_liCapturedImages.empty() )
		{
			*pImg = m_liCapturedImages.front();
			m_liCapturedImages.pop_front();
			return Status::READY;
		}
    }

	if ( GetMode() != Mode::Off )
		return Status::WAIT;

    return Status::DONE;
}

void SHCamera::Initialize()
{
	// Make sure we've wrapped up
	Finalize();

#if SH_USE_EDSDK
    // Try to get camera, clean up if fail
    EdsCameraListRef cameraList = nullptr;
    EdsCameraRef camera = nullptr;
    auto checkErr = [&cameraList, &camera]( EdsError err )
    {
        if ( err == EDS_ERR_OK )
            return;

        if ( cameraList )
            EdsRelease( cameraList );

        if ( camera )
            EdsRelease( camera );

        EdsTerminateSDK();
        throw std::runtime_error( "Error opening camera!" );
    };

    // Init EDSDK
    checkErr( EdsInitializeSDK() );

    // Enumerate cameras and get first
    EdsUInt32 count = 0;
    checkErr( EdsGetCameraList( &cameraList ) );
    checkErr( EdsGetChildCount( cameraList, &count ) );
    checkErr( EdsGetChildAtIndex( cameraList, 0, &camera ) );
	if ( camera == nullptr )
		checkErr( EDS_ERR_DEVICE_NOT_FOUND );

    // Free camera list
    EdsRelease( cameraList );
    cameraList = nullptr;
    
    // Get device info
    EdsDeviceInfo deviceInfo;
    checkErr( EdsGetDeviceInfo( camera, &deviceInfo ) );

    // Set Object Event Handler
    checkErr( EdsSetObjectEventHandler( camera, kEdsObjectEvent_All, SHCamera::handleObjectEvent, ( EdsVoid * ) this ) );

    // Set State Event Handler
    checkErr( EdsSetCameraStateEventHandler( camera, kEdsStateEvent_All, SHCamera::handleStateEvent, ( EdsVoid * ) this ) );

	// Set Property Event Handler
	checkErr( EdsSetPropertyEventHandler( camera, kEdsPropertyEvent_All, SHCamera::handlePropertyEvent, ( EdsVoid * ) this ) );

	// Construct camera model
	m_pCamModel.reset( new CameraModel( camera ) );

	// Open Session
	//m_CMDQueue.push_back( new CompositeCommand( m_pCamModel.get(), {
	//	new OpenSessionCommand( m_pCamModel.get() ),
	//	new GetPropertyCommand( m_pCamModel.get(), kEdsPropID_ProductName ),
	//} ) );

	// Set close CMD
	m_CMDQueue.SetCloseCommand( new CloseSessionCommand( m_pCamModel.get() ) );

	m_CMDQueue.push_back( new CompositeCommand( m_pCamModel.get(), {
		new OpenSessionCommand( m_pCamModel.get() ),
		new GetPropertyCommand( m_pCamModel.get(), kEdsPropID_ProductName )
	} ) );

#else
    auto checkErr = [](const int retVal, std::string strName){
        if (retVal != GP_OK){
            std::string strErrMsg = "Error: " + strName + " failed with error code " + std::to_string(retVal);
            throw std::runtime_error(strErrMsg);
        }
    };

    // gphoto2 context and camera
    m_pGPContext = gp_context_new();
    m_pGPCamera = nullptr;

    // Create and initialize camera
    checkErr(gp_camera_new(&m_pGPCamera), "Create Camera");
    checkErr(gp_camera_init(m_pGPCamera, m_pGPContext), "Init Camera");
#endif
}

void SHCamera::Finalize()
{
#if SH_USE_EDSDK
	// Let the CMD queue finish
	m_CMDQueue.waitTillCompletion();
	m_CMDQueue.clear( true );

	SetMode( Mode::Off );

	m_pCamModel.reset();
#else
    // Close camera
    gp_camera_exit(m_pGPCamera, m_pGPContext);

    // What about the context?
#endif

	// Stop capture thread
	if ( m_thCapture.joinable() )
		m_thCapture.join();
}

void SHCamera::SetMode( Mode mode )
{
#if SH_USE_EDSDK
	// We may start the thread if we're
	// switching from off to somthing else
	bool bStartThread( false );
	{
		// Lock while we read/write mode
		std::lock_guard<std::mutex> lgMode( m_muCamMode );

		// No change? get out
		if ( m_eMode == mode )
			return;

		// Init if we're off
		bStartThread = ( m_eMode == Mode::Off );

		// Handle mode transition
		switch ( mode )
		{
			case Mode::Off:
				// Cancel EVF if we are streaming
				if ( m_eMode == Mode::Streaming )
					m_CMDQueue.push_back( new EndEvfCommand( m_pCamModel.get() ) );
				// Close the session
				m_CMDQueue.push_back( new CloseSessionCommand( m_pCamModel.get() ) );
				break;
			case Mode::Streaming:
				// Post a start EVF command and an initial download command
				m_CMDQueue.push_back( new CompositeCommand( m_pCamModel.get(), {
					new StartEvfCommand( m_pCamModel.get() ),
					new GetPropertyCommand( m_pCamModel.get(), kEdsPropID_Evf_Mode ),
					new GetPropertyCommand( m_pCamModel.get(), kEdsPropID_Evf_OutputDevice ),
					new DownloadEvfCommand( m_pCamModel.get(), this ),
				} ) );
				break;
			case Mode::Capturing:
				
				// End streaming if necessary
				if ( m_eMode == Mode::Streaming )
					m_CMDQueue.push_back( new EndEvfCommand( m_pCamModel.get() ) );

				// Push back initial take picture command
				m_CMDQueue.push_back( new TakePictureCommand( m_pCamModel.get(), m_nShutterDuration ) );
				break;
			default:
				return;
		}

		// Store new mode (should this be done before commands are posted?)
		m_eMode = mode;
	}

	{
		// If we're changing modes, clear our list of captured images
		std::lock_guard<std::mutex> lgCapture( m_muCapture );
		m_liCapturedImages.clear();
	}

	// Start capture thread if necessary
	if ( bStartThread )
	{
		m_thCapture = std::thread( [this]()
		{
			threadProc();
		} );
	}
#endif
}

SHCamera::Mode SHCamera::GetMode()
{
	std::lock_guard<std::mutex> lgMode( m_muCamMode );
	return m_eMode;
}

void SHCamera::threadProc()
{
#if SH_USE_EDSDK && WIN32
	// When using the SDK from another thread in Windows, 
	// you must initialize the COM library by calling CoInitialize 
	::CoInitializeEx( NULL, COINIT_MULTITHREADED );
#endif

	// Continue until we get switched off
	for ( Mode eCurMode = GetMode(); eCurMode != Mode::Off; eCurMode = GetMode() )
	{
#if SH_USE_EDSDK
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		auto pCMD = m_CMDQueue.pop();
		if ( pCMD )
		{
			if ( pCMD->execute() == false )
			{
				std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
				m_CMDQueue.push_back( pCMD.release() );
			}
		}
#else
		CameraFile * pCamFile( nullptr );
		CameraFilePath camFilePath { 0 };

		checkErr( gp_camera_capture( m_pGPCamera, GP_CAPTURE_IMAGE, &camFilePath, m_pGPContext ), "Capture" );
		checkErr( gp_file_new( &pCamFile ), "Download File" );
		checkErr( gp_camera_file_get( m_pGPCamera, camFilePath.folder, camFilePath.name, GP_FILE_TYPE_NORMAL,
									  pCamFile, m_pGPContext ), "Download File" );

		void * pData( nullptr );
		size_t uDataSize( 0 );
		checkErr( gp_file_get_data_and_size( pCamFile, (const char **) &pData, &uDataSize ), "Get file and data size" );
#endif
	}

#if SH_USE_EDSDK
	// Close the command queue when the thread exits
	m_CMDQueue.clear( true );
#endif
}

#if SH_USE_EDSDK
// Object (usually image)  handling
EdsError SHCamera::handleObjectEvent_impl( EdsUInt32			inEvent,
                                         EdsBaseRef			inRef,
                                         EdsVoid *			inContext )
{
    switch ( inEvent )
    {
        // Download image request, should happen in response to
        // capture thread pressing camera button
        case kEdsObjectEvent_DirItemRequestTransfer:
        {
            // Post a download command with us as the receiver
            if ( inRef )
            {
				m_CMDQueue.push_back( new DownloadCommand( m_pCamModel.get(), inRef, this ) );
            }
        }
            break;

        // Object without the necessity is released
        default:
            if ( inRef != NULL )
                EdsRelease( inRef );
            break;
    }

    return EDS_ERR_OK;
}

// Handle shutdown state event
EdsError SHCamera::handleStateEvent_impl( EdsUInt32			inEvent,
                                        EdsUInt32			inParam,
                                        EdsVoid *			inContext )
{
    switch ( inEvent )
    {
        case kEdsStateEvent_Shutdown:
        {
            SetMode( Mode::Off );
        }
            break;
    }

    return EDS_ERR_OK;
}

// Static object event handler
/*static*/ EdsError EDSCALLBACK SHCamera::handleObjectEvent(
    EdsUInt32			inEvent,
    EdsBaseRef			inRef,
    EdsVoid *			inContext
)
{
    SHCamera * pCam = (SHCamera *) inContext;
    if ( pCam )
        return pCam->handleObjectEvent_impl( inEvent, inRef, inContext );
    return EDS_ERR_OK;
}

// Static state event handler
/*static*/ EdsError EDSCALLBACK SHCamera::handleStateEvent(
    EdsUInt32			inEvent,
    EdsUInt32			inParam,
    EdsVoid *			inContext
)
{
    SHCamera * pCam = (SHCamera *) inContext;
    if ( pCam )
        return pCam->handleStateEvent_impl( inEvent, inParam, inContext );
    return EDS_ERR_OK;
}

EdsError EDSCALLBACK SHCamera::handlePropertyEvent_impl(
	EdsUInt32			inEvent,
	EdsUInt32			inPropertyID,
	EdsUInt32			inParam,
	EdsVoid *			inContext
)
{
	switch ( inEvent )
	{
		case kEdsPropertyEvent_PropertyChanged:
			m_CMDQueue.push_back( new GetPropertyCommand( m_pCamModel.get(), inPropertyID ) );
			break;

		case kEdsPropertyEvent_PropertyDescChanged:
			m_CMDQueue.push_back( new GetPropertyDescCommand( m_pCamModel.get(), inPropertyID ) );
			break;
	}

	return EDS_ERR_OK;
}

// Static property event handler
/*static*/ EdsError EDSCALLBACK SHCamera::handlePropertyEvent(
	EdsUInt32			inEvent,
	EdsUInt32			inPropertyID,
	EdsUInt32			inParam,
	EdsVoid *			inContext
)
{
	SHCamera * pCam = (SHCamera *) inContext;
	if ( pCam )
		return pCam->handlePropertyEvent_impl( inEvent, inPropertyID, inParam, inContext );
	return EDS_ERR_OK;
}

bool SHCamera::handleCapturedImage( EdsDirectoryItemRef inRef )
{
	EdsError				err = EDS_ERR_OK;
	EdsStreamRef			stream = NULL;

	//Acquisition of the downloaded image information
	EdsDirectoryItemInfo	dirItemInfo;
	err = EdsGetDirectoryItemInfo( inRef, &dirItemInfo );

	// Make the file stream at the forwarding destination
	if ( err == EDS_ERR_OK )
	{
		std::string strFileName = m_strImgCapturePrefix + std::to_string( m_nImagesCaptured );
		err = EdsCreateFileStream( strFileName.c_str(), kEdsFileCreateDisposition_CreateAlways, kEdsAccess_ReadWrite, &stream );
		//err = EdsCreateMemoryStream( dirItemInfo.size, &stream );
	}
	else
		return false;

	//Download image
	if ( err == EDS_ERR_OK )
	{
		err = EdsDownload( inRef, dirItemInfo.size, stream );
	}
	else
	{
		EdsRelease( stream );
		return false;
	}

	// Forwarding completion
	if ( err == EDS_ERR_OK )
	{
		err = EdsDownloadComplete( inRef );
	}
	else
	{
		EdsRelease( stream );
		EdsRelease( inRef );
		return false;
	}

	//Release Item
	if ( inRef != NULL )
	{
		err = EdsRelease( inRef );
	}

	// If we've hit the limit, set ourselves to off and return
	if ( m_nImagesCaptured++ > m_nImageCaptureLimit )
	{
		SetMode( Mode::Off );
		return true;
	}

	// Enqueue the next take picture command if still capturing
	if ( GetMode() == Mode::Capturing )
		m_CMDQueue.push_back( new TakePictureCommand( m_pCamModel.get(), m_nShutterDuration ) );

	return true;
}

bool SHCamera::handleEvfImage()
{
	if ( m_pCamModel == nullptr )
		return true;

	if ( ( m_pCamModel->getEvfOutputDevice() & kEdsEvfOutputDevice_PC ) == 0 )
	{
		return true;
	}

	EdsError err = EDS_ERR_OK;
	
	EdsStreamRef evfStmRef( nullptr );
	err = EdsCreateMemoryStream( 2, &evfStmRef );
	if ( err != EDS_ERR_OK )
		return false;
	
	EdsImageRef imgRef( nullptr );
	err = EdsCreateEvfImageRef( evfStmRef, &imgRef );
	if ( err != EDS_ERR_OK )
	{
		EdsRelease( evfStmRef );
		return false;
	}

	// Download live view image data.
	err = EdsDownloadEvfImage( m_pCamModel->getCameraObject(), imgRef );

	// Convert JPG to img, post
	if ( err == EDS_ERR_OK )
	{
		// Cache the current mode here
		SHCamera::Mode appMode = GetMode();
		size_t nImages( 1 );

		// Get image data/size
		void * pData( nullptr );
		EdsUInt64 uDataSize( 0 );
		err = EdsGetLength( evfStmRef, &uDataSize );
		if ( err == EDS_ERR_OK )
		{
			err = EdsGetPointer( evfStmRef, &pData );
			if ( err == EDS_ERR_OK )
			{
				if ( uDataSize && pData )
				{
					// It seems like the camera gives me a JPG
					// that gets converted into a BGR24 image
					cv::Mat matJPG( 1, uDataSize, CV_8UC1, pData );
					cv::Mat matImg = cv::imdecode( matJPG, 1 );
					if ( matImg.empty() || matImg.type() != CV_8UC3 )
					{
						throw std::runtime_error( "Error decoding JPG image!" );
					}

					// convert to RGB
					// I actually don't know why I can't do everything
					// in BGR, but the images kept coming up black
					cv::cvtColor( matImg, matImg, CV_BGR2RGB );

					// Convert to single channel float
					cv::Mat imgGrey;
					cv::cvtColor( matImg, imgGrey, CV_BGR2GRAY );
					imgGrey.convertTo( matImg, CV_32FC1, 1.f / 0xFF );

					if ( nImages > 1 )
						m_liImageStack.emplace_back( std::move( matImg ) );
					else
						m_liImageStack = { matImg };
				}
			}
		}
		
		// Every nImages, collapse the stack
		if ( m_liImageStack.size() >= nImages )
		{
			// Average the pixels of every image in the stack
			cv::Mat avgImg( m_liImageStack.front().rows, m_liImageStack.front().cols, m_liImageStack.front().type() );
			avgImg.setTo( 0 );
			const float fDiv = 1.f / m_liImageStack.size();
			for ( cv::Mat& img : m_liImageStack )
				avgImg += fDiv * img;


			// Lock mutex, post image
			{
				std::lock_guard<std::mutex> lg( m_muCapture );

				// Pop off oldest 10 from front
				if ( m_liCapturedImages.size() > 10 )
					m_liCapturedImages.erase( m_liCapturedImages.begin(), std::next( m_liCapturedImages.begin(), 10 ) );
#if SH_CUDA
				img_t imgUpload;
				imgUpload.upload( avgImg );
				m_liCapturedImages.push_back( imgUpload );
#else
				m_liCapturedImages.push_back( avgImg );
#endif
			}

			// Clear stack
			m_liImageStack.clear();
		}


		if ( evfStmRef )
			EdsRelease( evfStmRef );
		if ( imgRef )
			EdsRelease( imgRef );

		// Enqueue the next download command if still streaming, get out
		if ( GetMode() == Mode::Streaming )
			m_CMDQueue.push_back( new DownloadEvfCommand( m_pCamModel.get(), this ) );

		return true;
	}

	// Something bad happened
	if ( evfStmRef )
		EdsRelease( evfStmRef );
	if ( imgRef )
		EdsRelease( imgRef );

	return false;
}

#endif
#endif
