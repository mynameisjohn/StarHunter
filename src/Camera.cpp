#if SH_CAMERA

#include <libraw/libraw.h>
#include <chrono>

#include "Util.h"
#include "Camera.h"


#ifdef WIN32

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

SHCamera::SHCamera( std::string strNamePrefix, int nImagesToCapture ) :
	m_eMode( Mode::Off ),
	m_strImgCapturePrefix( strNamePrefix ),
	m_nImageCaptureLimit( nImagesToCapture ),
	m_nImagesCaptured(0)
#ifndef WIN32
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
	img_t imgRet;

    {
		std::lock_guard<std::mutex> lg( m_muCapture );
		if ( !m_liCapturedImages.empty() )
		{
			imgRet = m_liCapturedImages.front();
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

#ifdef WIN32
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

	// Turn us on? 
	SetMode( Mode::Streaming );

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
#ifdef WIN32
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
	// We may start the thread if we're
	// switching from off to somthing else
	bool bStartThread( false );
	{
		// Lock while we read/write mode
		std::lock_guard<std::mutex> lgMode( m_muCamMode );

		// No change? get out
		if ( m_eMode == mode )
			return;

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
				//// Init if we're off
				//if ( m_eMode == Mode::Off )
				//	bStartThread = true;
				// Post a start EVF command and an initial download command
				m_CMDQueue.push_back( new CompositeCommand( m_pCamModel.get(), {
					new StartEvfCommand( m_pCamModel.get() ),
					new GetPropertyCommand( m_pCamModel.get(), kEdsPropID_Evf_Mode ),
					new GetPropertyCommand( m_pCamModel.get(), kEdsPropID_Evf_OutputDevice ),
					new DownloadEvfCommand( m_pCamModel.get(), this ),
				} ) );
				break;
			case Mode::Capturing:
				// Init if we're off
				//if ( m_eMode == Mode::Off )
				//	bStartThread = true;
				// End streaming if necessary
				if ( m_eMode == Mode::Streaming )
					m_CMDQueue.push_back( new EndEvfCommand( m_pCamModel.get() ) );
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
}

SHCamera::Mode SHCamera::GetMode()
{
	std::lock_guard<std::mutex> lgMode( m_muCamMode );
	return m_eMode;
}

void SHCamera::threadProc()
{
#ifdef WIN32
	// When using the SDK from another thread in Windows, 
	// you must initialize the COM library by calling CoInitialize 
	::CoInitializeEx( NULL, COINIT_MULTITHREADED );

	// Continue until we get switched off
	for ( Mode eCurMode = GetMode(); eCurMode != Mode::Off; eCurMode = GetMode() )
	{
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
	}

	// Close the command queue when the thread exits
	m_CMDQueue.clear( true );
#else
	bool bRunning_tl = m_abRunning.load();
	while ( bRunning_tl )
	{
		CameraFile * pCamFile( nullptr );
		CameraFilePath camFilePath { 0 };

		checkErr( gp_camera_capture( m_pGPCamera, GP_CAPTURE_IMAGE, &camFilePath, m_pGPContext ), "Capture" );
		checkErr( gp_file_new( &pCamFile ), "Download File" );
		checkErr( gp_camera_file_get( m_pGPCamera, camFilePath.folder, camFilePath.name, GP_FILE_TYPE_NORMAL,
									  pCamFile, m_pGPContext ), "Download File" );

		void * pData( nullptr );
		size_t uDataSize( 0 );
		checkErr( gp_file_get_data_and_size( pCamFile, (const char **) &pData, &uDataSize ), "Get file and data size" );
		bRunning_tl = m_abRunning.load();
	}
}
#endif
}

#ifdef WIN32
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
		m_CMDQueue.push_back( new TakePictureCommand( m_pCamModel.get() ) );

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
		size_t nImages( 10 );

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

					// 3 channel float (should be single)
					matImg.convertTo( matImg, CV_32FC3, 1.f / 0xFF );

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

bool EndEvfCommand::execute()
{
	EdsError err = EDS_ERR_OK;


	// Get the current output device.
	EdsUInt32 device = _model->getEvfOutputDevice();

	// Do nothing if the remote live view has already ended.
	if ( ( device & kEdsEvfOutputDevice_PC ) == 0 )
	{
		return true;
	}

	// Get depth of field status.
	EdsUInt32 depthOfFieldPreview = _model->getEvfDepthOfFieldPreview();

	// Release depth of field in case of depth of field status.
	if ( depthOfFieldPreview != 0 )
	{
		depthOfFieldPreview = 0;
		err = EdsSetPropertyData( _model->getCameraObject(), kEdsPropID_Evf_DepthOfFieldPreview, 0, sizeof( depthOfFieldPreview ), &depthOfFieldPreview );

		// Standby because commands are not accepted for awhile when the depth of field has been released.
		if ( err == EDS_ERR_OK )
		{
			Sleep( 500 );
		}
	}

	// Change the output device.
	if ( err == EDS_ERR_OK )
	{
		device &= ~kEdsEvfOutputDevice_PC;
		err = EdsSetPropertyData( _model->getCameraObject(), kEdsPropID_Evf_OutputDevice, 0, sizeof( device ), &device );
	}

	//Notification of error
	if ( err != EDS_ERR_OK )
	{
		// It retries it at device busy
		if ( err == EDS_ERR_DEVICE_BUSY )
		{
			return false;
		}

		// Retry until successful.
		return false;
	}

	return true;
}

bool DownloadEvfCommand::execute()
{
	if ( m_pReceiver )
	{
		return m_pReceiver->handleEvfImage();
	}

	return true;
}

bool StartEvfCommand::execute()
{
	EdsError err = EDS_ERR_OK;

	/// Change settings because live view cannot be started
	/// when camera settings are set to gdo not perform live view.h
	EdsUInt32 evfMode = _model->getEvfMode();
	err = EdsGetPropertyData( _model->getCameraObject(), kEdsPropID_Evf_Mode, 0, sizeof( evfMode ), &evfMode );

	if ( err == EDS_ERR_OK )
	{
		evfMode = _model->getEvfMode();
		if ( evfMode == 0 )
		{
			evfMode = 1;

			// Set to the camera.
			err = EdsSetPropertyData( _model->getCameraObject(), kEdsPropID_Evf_Mode, 0, sizeof( evfMode ), &evfMode );
		}
	}


	if ( err == EDS_ERR_OK )
	{
		// Get the current output device.
		EdsUInt32 device = _model->getEvfOutputDevice();

		// Set the PC as the current output device.
		device |= kEdsEvfOutputDevice_PC;

		// Set to the camera.
		err = EdsSetPropertyData( _model->getCameraObject(), kEdsPropID_Evf_OutputDevice, 0, sizeof( device ), &device );
	}

	//Notification of error
	if ( err != EDS_ERR_OK )
	{
		// It doesn't retry it at device busy
		if ( err == EDS_ERR_DEVICE_BUSY )
		{
			return false;
		}
	}

	return true;
}

bool TakePictureCommand::execute()
{
	EdsError err = EDS_ERR_OK;
	bool	 locked = false;

	//Taking a picture
	err = EdsSendCommand( _model->getCameraObject(), kEdsCameraCommand_PressShutterButton, kEdsCameraCommand_ShutterButton_Completely );
	EdsSendCommand( _model->getCameraObject(), kEdsCameraCommand_PressShutterButton, kEdsCameraCommand_ShutterButton_OFF );


	//Notification of error
	if ( err != EDS_ERR_OK )
	{
		// It retries it at device busy
		if ( err == EDS_ERR_DEVICE_BUSY )
		{
			return true;
		}
	}

	return true;
}

bool DownloadCommand::execute()
{
	// Delegate to receiver if we have one, otherwise download and get out
	if ( m_pReceiver )
		return m_pReceiver->handleCapturedImage( _directoryItem );

	// Execute command 	

	EdsError				err = EDS_ERR_OK;
	EdsStreamRef			stream = NULL;

	//Acquisition of the downloaded image information
	EdsDirectoryItemInfo	dirItemInfo;
	err = EdsGetDirectoryItemInfo( _directoryItem, &dirItemInfo );

	//Make the file stream at the forwarding destination
	if ( err == EDS_ERR_OK )
	{
		err = EdsCreateFileStream( dirItemInfo.szFileName, kEdsFileCreateDisposition_CreateAlways, kEdsAccess_ReadWrite, &stream );
	}

	//Download image
	if ( err == EDS_ERR_OK )
	{
		err = EdsDownload( _directoryItem, dirItemInfo.size, stream );
	}

	//Forwarding completion
	if ( err == EDS_ERR_OK )
	{
		err = EdsDownloadComplete( _directoryItem );
	}

	//Release Item
	if ( _directoryItem != NULL )
	{
		err = EdsRelease( _directoryItem );
		_directoryItem = NULL;
	}

	//Release stream
	if ( stream != NULL )
	{
		err = EdsRelease( stream );
		stream = NULL;
	}

	return true;
}

DownloadCommand::DownloadCommand( CameraModel *model, EdsDirectoryItemRef dirItem, Receiver * pReceiver /*= nullptr */ )
	: _directoryItem( dirItem ), m_pReceiver( pReceiver ), Command( model )
{}

DownloadCommand::~DownloadCommand()
{
	//Release item
	if ( _directoryItem != NULL )
	{
		EdsRelease( _directoryItem );
		_directoryItem = NULL;
	}
}

OpenSessionCommand::OpenSessionCommand( CameraModel *model ) : Command( model ) {}

bool OpenSessionCommand::execute()
{
	EdsError err = EDS_ERR_OK;
	bool	 locked = false;

	//The communication with the camera begins
	err = EdsOpenSession( _model->getCameraObject() );

	//Preservation ahead is set to PC
	if ( err == EDS_ERR_OK )
	{
		EdsUInt32 saveTo = kEdsSaveTo_Host;
		err = EdsSetPropertyData( _model->getCameraObject(), kEdsPropID_SaveTo, 0, sizeof( saveTo ), &saveTo );
	}

	//UI lock
	if ( err == EDS_ERR_OK )
	{
		err = EdsSendStatusCommand( _model->getCameraObject(), kEdsCameraStatusCommand_UILock, 0 );
	}

	if ( err == EDS_ERR_OK )
	{
		locked = true;
	}

	if ( err == EDS_ERR_OK )
	{
		EdsCapacity capacity = { 0x7FFFFFFF, 0x1000, 1 };
		err = EdsSetCapacity( _model->getCameraObject(), capacity );
	}

	//It releases it when locked
	if ( locked )
	{
		EdsSendStatusCommand( _model->getCameraObject(), kEdsCameraStatusCommand_UIUnLock, 0 );
	}

	return true;
}

CloseSessionCommand::CloseSessionCommand( CameraModel *model ) : Command( model ) {}

bool CloseSessionCommand::execute()
{
	EdsError err = EDS_ERR_OK;

	//The communication with the camera is ended
	err = EdsCloseSession( _model->getCameraObject() );

	return true;
}

GetPropertyCommand::GetPropertyCommand( CameraModel *model, EdsPropertyID propertyID )
	:_propertyID( propertyID ), Command( model )
{}

bool GetPropertyCommand::execute()
{
	EdsError err = EDS_ERR_OK;

	//Get property value
	if ( err == EDS_ERR_OK )
	{
		err = getProperty( _propertyID );
	}

	//Notification of error
	if ( err != EDS_ERR_OK )
	{
		// It retries it at device busy
		if ( ( err & EDS_ERRORID_MASK ) == EDS_ERR_DEVICE_BUSY )
		{
			return false;
		}
	}

	return true;
}

EdsError GetPropertyCommand::getProperty( EdsPropertyID propertyID )
{
	EdsError err = EDS_ERR_OK;
	EdsDataType	dataType = kEdsDataType_Unknown;
	EdsUInt32   dataSize = 0;


	if ( propertyID == kEdsPropID_Unknown )
	{
		//If unknown is returned for the property ID , the required property must be retrieved again
		if ( err == EDS_ERR_OK ) err = getProperty( kEdsPropID_AEModeSelect );
		if ( err == EDS_ERR_OK ) err = getProperty( kEdsPropID_Tv );
		if ( err == EDS_ERR_OK ) err = getProperty( kEdsPropID_Av );
		if ( err == EDS_ERR_OK ) err = getProperty( kEdsPropID_ISOSpeed );
		if ( err == EDS_ERR_OK ) err = getProperty( kEdsPropID_MeteringMode );
		if ( err == EDS_ERR_OK ) err = getProperty( kEdsPropID_ExposureCompensation );
		if ( err == EDS_ERR_OK ) err = getProperty( kEdsPropID_ImageQuality );

		return err;
	}

	//Acquisition of the property size
	if ( err == EDS_ERR_OK )
	{
		err = EdsGetPropertySize( _model->getCameraObject(),
								  propertyID,
								  0,
								  &dataType,
								  &dataSize );
	}

	if ( err == EDS_ERR_OK )
	{

		if ( dataType == kEdsDataType_UInt32 )
		{
			EdsUInt32 data;

			//Acquisition of the property
			err = EdsGetPropertyData( _model->getCameraObject(),
									  propertyID,
									  0,
									  dataSize,
									  &data );

			//Acquired property value is set
			if ( err == EDS_ERR_OK )
			{
				_model->setPropertyUInt32( propertyID, data );
			}
		}

		if ( dataType == kEdsDataType_String )
		{

			EdsChar str[EDS_MAX_NAME];
			//Acquisition of the property
			err = EdsGetPropertyData( _model->getCameraObject(),
									  propertyID,
									  0,
									  dataSize,
									  str );

			//Acquired property value is set
			if ( err == EDS_ERR_OK )
			{
				_model->setPropertyString( propertyID, str );
			}
		}
		if ( dataType == kEdsDataType_FocusInfo )
		{
			EdsFocusInfo focusInfo;
			//Acquisition of the property
			err = EdsGetPropertyData( _model->getCameraObject(),
									  propertyID,
									  0,
									  dataSize,
									  &focusInfo );

			//Acquired property value is set
			if ( err == EDS_ERR_OK )
			{
				_model->setFocusInfo( focusInfo );
			}
		}
	}

	return err;
}

GetPropertyDescCommand::GetPropertyDescCommand( CameraModel *model, EdsPropertyID propertyID )
	:_propertyID( propertyID ), Command( model )
{}

EdsError GetPropertyDescCommand::getPropertyDesc( EdsPropertyID propertyID )
{
	EdsError  err = EDS_ERR_OK;
	EdsPropertyDesc	 propertyDesc = { 0 };

	if ( propertyID == kEdsPropID_Unknown )
	{
		//If unknown is returned for the property ID , the required property must be retrieved again
		if ( err == EDS_ERR_OK ) err = getPropertyDesc( kEdsPropID_AEModeSelect );
		if ( err == EDS_ERR_OK ) err = getPropertyDesc( kEdsPropID_Tv );
		if ( err == EDS_ERR_OK ) err = getPropertyDesc( kEdsPropID_Av );
		if ( err == EDS_ERR_OK ) err = getPropertyDesc( kEdsPropID_ISOSpeed );
		if ( err == EDS_ERR_OK ) err = getPropertyDesc( kEdsPropID_MeteringMode );
		if ( err == EDS_ERR_OK ) err = getPropertyDesc( kEdsPropID_ExposureCompensation );
		if ( err == EDS_ERR_OK ) err = getPropertyDesc( kEdsPropID_ImageQuality );

		return err;
	}

	//Acquisition of value list that can be set
	if ( err == EDS_ERR_OK )
	{
		err = EdsGetPropertyDesc( _model->getCameraObject(),
								  propertyID,
								  &propertyDesc );
	}

	//The value list that can be the acquired setting it is set		
	if ( err == EDS_ERR_OK )
	{
		_model->setPropertyDesc( propertyID, &propertyDesc );
	}

	////Update notification
	//if(err == EDS_ERR_OK)
	//{
	//	CameraEvent e("PropertyDescChanged", &propertyID);
	//	_model->notifyObservers(&e);
	//}

	return err;
}

bool GetPropertyDescCommand::execute()
{
	EdsError err = EDS_ERR_OK;
	bool	 locked = false;

	//Get property
	if ( err == EDS_ERR_OK )
	{
		err = getPropertyDesc( _propertyID );
	}

	//It releases it when locked
	if ( locked )
	{
		EdsSendStatusCommand( _model->getCameraObject(), kEdsCameraStatusCommand_UIUnLock, 0 );
	}


	//Notification of error
	if ( err != EDS_ERR_OK )
	{
		// It retries it at device busy
		if ( ( err & EDS_ERRORID_MASK ) == EDS_ERR_DEVICE_BUSY )
		{
			return false;
		}
	}

	return true;
}

DownloadEvfCommand::DownloadEvfCommand( CameraModel *model, Receiver * pReceiver /*= nullptr*/ ) :
	Command( model ),
	m_pReceiver( pReceiver )
{}

SleepCommand::SleepCommand( CameraModel *model, uint32_t uSleepDur ) : Command( model ), m_uSleepDur( uSleepDur ) {}

bool SleepCommand::execute()
{
	std::this_thread::sleep_for( std::chrono::milliseconds( m_uSleepDur ) );
	return true;
}

CommandQueue::CommandQueue()
{

}

CommandQueue::~CommandQueue()
{
	clear();
}

void CommandQueue::clear( bool bClose )
{
	std::lock_guard<std::mutex> lg( m_muCommandMutex );
	m_liCommands.clear();

	if ( bClose && m_pCloseCommand )
	{
		m_pCloseCommand->execute();
		m_pCloseCommand.reset();
	}
}

CmdPtr CommandQueue::pop()
{
	std::lock_guard<std::mutex> lg( m_muCommandMutex );
	if ( m_liCommands.empty() )
		return nullptr;

	auto ret = std::move( m_liCommands.front() );
	m_liCommands.pop_front();
	return std::move( ret );
}

void CommandQueue::push_back( Command * pCMD )
{
	std::lock_guard<std::mutex> lg( m_muCommandMutex );
	m_liCommands.emplace_back( pCMD );
}

void CommandQueue::SetCloseCommand( Command * pCMD )
{
	std::lock_guard<std::mutex> lg( m_muCommandMutex );

	if ( pCMD )
		m_pCloseCommand = CmdPtr( pCMD );
	else
		m_pCloseCommand.reset();
}

void CommandQueue::waitTillCompletion()
{
	volatile bool bSpin = true;
	while ( bSpin )
	{
		std::this_thread::sleep_for( std::chrono::milliseconds( 250 ) );
		std::lock_guard<std::mutex> lg( m_muCommandMutex );
		bSpin = !m_liCommands.empty();
	}
}

#endif

#endif