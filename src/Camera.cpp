
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

SHCamera::SHCamera() :
#ifdef WIN32
    m_CamRef( nullptr ),
    m_ImgRefToDownload( nullptr )
#else
    m_pGPContext(nullptr),
    m_pGPCamera(nullptr)
#endif
{}

SHCamera::~SHCamera()
{
    Finalize();
}

ImageSource::Status SHCamera::GetStatus() const
{
    if ( m_abCapture.load() )
    {
        if ( m_liCapturedImages.empty() )
            return ImageSource::Status::WAIT;
        return ImageSource::Status::READY;
    }

    return ImageSource::Status::DONE;
}

img_t SHCamera::GetNextImage()
{
	img_t imgRet;

    {
        std::lock_guard<std::mutex> lg( m_muCapture );
        if ( !m_liCapturedImages.empty() )
            imgRet = m_liCapturedImages.front();
        m_liCapturedImages.pop_front();
    }

    return imgRet;
}

void SHCamera::Initialize()
{   
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

    // Free camera list
    EdsRelease( cameraList );
    cameraList = nullptr;
    
    // Get device info
    EdsDeviceInfo deviceInfo;
    checkErr( EdsGetDeviceInfo( camera, &deviceInfo ) );

    //Set Object Event Handler
    checkErr( EdsSetObjectEventHandler( camera, kEdsObjectEvent_All, SHCamera::handleObjectEvent, ( EdsVoid * ) this ) );

    //Set State Event Handler
    checkErr( EdsSetCameraStateEventHandler( camera, kEdsStateEvent_All, SHCamera::handleStateEvent, ( EdsVoid * ) this ) );

    m_CamRef = camera;

    //The communication with the camera begins
    checkErr( EdsOpenSession( m_CamRef ) );

    // Preservation ahead is set to PC
    EdsUInt32 saveTo = kEdsSaveTo_Host;
    checkErr( EdsSetPropertyData( m_CamRef, kEdsPropID_SaveTo, 0, sizeof( saveTo ), &saveTo ) );

    // UI lock
    checkErr( EdsSendStatusCommand( m_CamRef, kEdsCameraStatusCommand_UILock, 0 ) );

    EdsCapacity capacity = { 0x7FFFFFFF, 0x1000, 1 };
    checkErr( EdsSetCapacity( m_CamRef, capacity ) );

    // It releases it when locked
    checkErr( EdsSendStatusCommand( m_CamRef, kEdsCameraStatusCommand_UIUnLock, 0 ) );
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

    // Start capture thread
    m_abCapture.store( true );
    m_thCapture = std::thread( [this]()
    {
        threadProc();
    } );
}

void SHCamera::Finalize()
{
    // Stop capture thread
    m_abCapture.store( false );

    m_thCapture.join();

#ifdef WIN32
    // Free EDS stuff
    if ( m_ImgRefToDownload )
        EdsRelease( m_ImgRefToDownload );
    
    if ( m_CamRef )
        EdsRelease( m_CamRef );
#else
    // Close camera
    gp_camera_exit(m_pGPCamera, m_pGPContext);

    // What about the context?
#endif
}

void SHCamera::threadProc()
{
#ifdef WIN32
    // Error check
    auto checkErr = []( EdsError e )
    {
        if ( e == EDS_ERR_OK )
            return;

        throw std::runtime_error( "Error opening camera!" );
    };
    // When using the SDK from another thread in Windows, 
    // you must initialize the COM library by calling CoInitialize 
    ::CoInitializeEx( NULL, COINIT_MULTITHREADED );
#endif

    while ( m_abCapture.load() )
    {
#ifdef WIN32
        EdsError err( EDS_ERR_OK );
        EdsError * pErr( &err );
        auto checkErr = [pErr]( EdsError err )
        {
            *pErr = err;
            if ( err == EDS_ERR_OK )
                return;

            throw std::runtime_error( "Error taking picture!" );
        };

        // Press shutter to take a picture
        try
        {
            checkErr( EdsSendCommand( m_CamRef, kEdsCameraCommand_PressShutterButton, kEdsCameraCommand_ShutterButton_Completely ) );
            checkErr( EdsSendCommand( m_CamRef, kEdsCameraCommand_PressShutterButton, kEdsCameraCommand_ShutterButton_OFF ) );
        }
        // Notification of error
        catch ( std::runtime_error e )
        {
            // We can retry if it's busy, otherwise something bad has happened
            if ( err != EDS_ERR_DEVICE_BUSY )
            {
                throw std::runtime_error( "Error taking picture!" );
            }
        }

        // Wait for response
        std::unique_lock<std::mutex> lk( m_muCamSync );
        m_cvCamSync.wait( lk );

        // Acquisition of the downloaded image information
        EdsDirectoryItemInfo	dirItemInfo;
        checkErr( EdsGetDirectoryItemInfo( m_ImgRefToDownload, &dirItemInfo ) );

        // Store in a memory stream
        EdsStreamRef stream = nullptr;
        checkErr( EdsCreateMemoryStream( dirItemInfo.size, &stream ) );

        // Download image
        checkErr( EdsDownload( m_ImgRefToDownload, dirItemInfo.size, stream ) );

        // Forwarding completion
        checkErr( EdsDownloadComplete( m_ImgRefToDownload ) );

        // Release Item
        if ( m_ImgRefToDownload != nullptr )
        {
            EdsRelease( m_ImgRefToDownload );
            m_ImgRefToDownload = nullptr;
        }

        // Get stream info
        void * pData( nullptr );
        EdsUInt64 uDataSize( 0 );
        checkErr( EdsGetLength( stream, &uDataSize ) );
        checkErr( EdsGetPointer( stream, &pData ) );
#else
        CameraFile * pCamFile(nullptr);
        CameraFilePath camFilePath{0};

        checkErr(gp_camera_capture(m_pGPCamera, GP_CAPTURE_IMAGE, &camFilePath, m_pGPContext), "Capture");
        checkErr(gp_file_new(&pCamFile), "Download File");
        checkErr(gp_camera_file_get(m_pGPCamera, camFilePath.folder, camFilePath.name, GP_FILE_TYPE_NORMAL,
                    pCamFile, m_pGPContext), "Download File");

        void * pData( nullptr );
        size_t uDataSize( 0 );
        checkErr(gp_file_get_data_and_size(pCamFile, (const char **)&pData, &uDataSize), "Get file and data size");
#endif // WIN32

        // Create cv mat from raw image and store
        cv::Mat imgGrey = Raw2Img( pData, uDataSize );
        std::lock_guard<std::mutex> lg( m_muCapture );
        m_liCapturedImages.push_back( imgGrey );

        

        // Sleep for a bit every iteration
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
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
            // Notify capture thread to continue
            if ( inRef )
            {
                m_ImgRefToDownload = inRef;
                m_cvCamSync.notify_one();
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
            m_abCapture.store( false );
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
#endif
