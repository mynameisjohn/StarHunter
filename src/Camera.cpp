
#include <libraw.h>

#include "Camera.h"

#include <chrono>

Camera::Camera() :
    m_CamRef( nullptr )
{}

Camera::~Camera()
{
    Finalize();
}

ImageSource::Status Camera::GetStatus() const
{
    if ( m_abCapture.load() )
    {
        if ( m_liCapturedImages.empty() )
            return ImageSource::Status::WAIT;
        return ImageSource::Status::READY;
    }

    return ImageSource::Status::DONE;
}

cv::Mat Camera::GetNextImage()
{
    cv::Mat imgRet;

    {
        std::lock_guard<std::mutex> lg( m_muCapture );
        if ( !m_liCapturedImages.empty() )
            imgRet = m_liCapturedImages.front();
        m_liCapturedImages.pop_front();
    }

    return imgRet;
}

void Camera::Initialize()
{   
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
    checkErr( EdsSetObjectEventHandler( camera, kEdsObjectEvent_All, Camera::handleObjectEvent, ( EdsVoid * ) this ) );

    //Set State Event Handler
    checkErr( EdsSetCameraStateEventHandler( camera, kEdsStateEvent_All, Camera::handleStateEvent, ( EdsVoid * ) this ) );

    m_CamRef = camera;

    m_abCapture.store( true );
    m_thCapture = std::thread( [this]()
    {
        threadProc();
    } );
}

void Camera::Finalize()
{
    m_abCapture.store( false );

    std::mutex m_muImageDownload;
    for ( EdsDirectoryItemRef& directoryItem : m_liImagesToDownload )
    {
        if ( directoryItem )
            EdsRelease( directoryItem );
    }
        
    if ( m_CamRef )
        EdsRelease( m_CamRef );
}

void Camera::threadProc()
{
    EdsError err( EDS_ERR_OK );
    auto checkErr = [&err]( EdsError rErr )
    {
        err = rErr;
        if ( err == EDS_ERR_OK )
            return;

        throw std::runtime_error( "Error opening camera!" );
    };

    // When using the SDK from another thread in Windows, 
    // you must initialize the COM library by calling CoInitialize 
    ::CoInitializeEx( NULL, COINIT_MULTITHREADED );

    //The communication with the camera begins
    checkErr( EdsOpenSession( m_CamRef ) );

    //Preservation ahead is set to PC
    EdsUInt32 saveTo = kEdsSaveTo_Host;
    checkErr( EdsSetPropertyData( m_CamRef, kEdsPropID_SaveTo, 0, sizeof( saveTo ), &saveTo ) );

    //UI lock
    checkErr( EdsSendStatusCommand( m_CamRef, kEdsCameraStatusCommand_UILock, 0 ) );

    EdsCapacity capacity = { 0x7FFFFFFF, 0x1000, 1 };
    checkErr( EdsSetCapacity( m_CamRef, capacity ) );

    //It releases it when locked
    checkErr( EdsSendStatusCommand( m_CamRef, kEdsCameraStatusCommand_UIUnLock, 0 ) );

    while ( m_abCapture.load() )
    {
        EdsError err = EDS_ERR_OK;

        // Press shutter to take a picture
        try
        {
            checkErr( EdsSendCommand( m_CamRef, kEdsCameraCommand_PressShutterButton, kEdsCameraCommand_ShutterButton_Completely ) );
            checkErr( EdsSendCommand( m_CamRef, kEdsCameraCommand_PressShutterButton, kEdsCameraCommand_ShutterButton_OFF ) );
        }

        catch ( std::runtime_error e )
        {
            //Notification of error
            if ( err != EDS_ERR_OK )
            {
                // We can retry if it's busy
                if ( err != EDS_ERR_DEVICE_BUSY )
                {
                    // But otherwise something bad has happened
                    throw std::runtime_error( "Error taking picture!" );
                }
            }
        }

        // See if we have any images to download
        std::list<EdsDirectoryItemRef> liImagesToDownload;
        {
            std::lock_guard<std::mutex> lg( m_muImageDownload );
            if ( m_liImagesToDownload.empty() == false )
            {
                liImagesToDownload.splice( liImagesToDownload.begin(), m_liImagesToDownload );
            }
        }

        // Download any images
        std::list<cv::Mat> liNewImages;
        for ( EdsDirectoryItemRef& directoryItem : liImagesToDownload )
        {
            EdsStreamRef			stream = NULL;

            // Acquisition of the downloaded image information
            EdsDirectoryItemInfo	dirItemInfo;
            checkErr( EdsGetDirectoryItemInfo( directoryItem, &dirItemInfo ) );
            
            // Store in a memory stream
            checkErr( EdsCreateMemoryStream( dirItemInfo.size, &stream ) );
            
            // Download image
            checkErr( EdsDownload( directoryItem, dirItemInfo.size, stream ) );
            
            // Forwarding completion
            checkErr( EdsDownloadComplete( directoryItem ) );

            //Release Item
            if ( directoryItem != NULL )
            {
                err = EdsRelease( directoryItem );
                directoryItem = NULL;
            }

            // Get stream info
            void * pData( nullptr );
            EdsUInt64 uDataSize( 0 );
            checkErr( EdsGetLength( stream, &uDataSize ) );
            checkErr( EdsGetPointer( stream, &pData ) );

            // Open the CR2 file with LibRaw, unpack, and create image
            LibRaw lrProc;
            assert( LIBRAW_SUCCESS == lrProc.open_buffer( pData, uDataSize ) );
            assert( LIBRAW_SUCCESS == lrProc.unpack() );
            assert( LIBRAW_SUCCESS == lrProc.raw2image() );

            // Get image dimensions
            int width = lrProc.imgdata.sizes.iwidth;
            int height = lrProc.imgdata.sizes.iheight;

            // Create a buffer of ushorts containing the pixel values of the
            // "BG Bayered" image (even rows are RGRGRG..., odd are GBGBGB...)'
            if ( m_vBayerDataBuffer.empty() )
                m_vBayerDataBuffer.resize( width*height );
            else
                std::fill( m_vBayerDataBuffer.begin(), m_vBayerDataBuffer.end(), 0 );

            std::vector<uint16_t>::iterator itBayer = m_vBayerDataBuffer.begin();
            for ( int y = 0; y < height; y++ )
            {
                for ( int x = 0; x < width; x++ )
                {
                    // Get pixel idx
                    int idx = y * width + x;

                    // Each pixel is an array of 4 shorts rgbg
                    ushort * uRGBG = lrProc.imgdata.image[idx];
                    int rowMod = ( y ) % 2; // 0 if even, 1 if odd
                    int colMod = ( x + rowMod ) % 2; // 1 if even, 0 if odd
                    int clrIdx = 2 * rowMod + colMod;
                    ushort val = uRGBG[clrIdx] << 4;
                    *itBayer++ = val;
                }
            }

            // Get rid of libraw image, construct openCV mat
            lrProc.recycle();
            cv::Mat imgBayer( height, width, CV_16UC1, m_vBayerDataBuffer.data() );

            // Debayer image, get output
            cv::Mat imgDeBayer;
            cv::cvtColor( imgBayer, imgDeBayer, CV_BayerBG2BGR );

            assert( imgDeBayer.type() == CV_16UC3 );

            // Convert to grey
            cv::Mat imgGrey;
            cv::cvtColor( imgDeBayer, imgGrey, cv::COLOR_RGB2GRAY );

            // Store the image
            std::lock_guard<std::mutex> lg( m_muCapture );
            m_liCapturedImages.push_back( imgGrey );
        }

        // Sleep for a bit every iteration
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

EdsError Camera::handleObjectEvent_impl( EdsUInt32			inEvent,
                                         EdsBaseRef			inRef,
                                         EdsVoid *			inContext )
{
    switch ( inEvent )
    {
        case kEdsObjectEvent_DirItemRequestTransfer:
        {
            if ( inRef )
            {
                std::lock_guard<std::mutex> lg( m_muImageDownload );
                m_liImagesToDownload.push_back( (EdsDirectoryItemRef) inRef );
            }
        }
            break;

        default:
            //Object without the necessity is released
            if ( inRef != NULL )
                EdsRelease( inRef );
            break;
    }

    return EDS_ERR_OK;
}

EdsError Camera::handleStateEvent_impl( EdsUInt32			inEvent,
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

/*static*/ EdsError EDSCALLBACK Camera::handleObjectEvent(
    EdsUInt32			inEvent,
    EdsBaseRef			inRef,
    EdsVoid *			inContext
)
{
    Camera * pCam = (Camera *) inContext;
    if ( pCam )
        return pCam->handleObjectEvent_impl( inEvent, inRef, inContext );
    return EDS_ERR_OK;
}

/*static*/ EdsError EDSCALLBACK Camera::handleStateEvent(
    EdsUInt32			inEvent,
    EdsUInt32			inParam,
    EdsVoid *			inContext
)
{
    Camera * pCam = (Camera *) inContext;
    if ( pCam )
        return pCam->handleStateEvent_impl( inEvent, inParam, inContext );
    return EDS_ERR_OK;
}