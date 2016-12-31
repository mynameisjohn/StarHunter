#include <libraw/libraw.h>

#include "Command.h"

#ifdef WIN32

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

	// Press shutter button
	err = EdsSendCommand( _model->getCameraObject(), kEdsCameraCommand_PressShutterButton, kEdsCameraCommand_ShutterButton_Completely );

	// If we have a shutter duration specified, sleep for that amount
	if ( m_nShutterDurationInSeconds > 0 )
		std::this_thread::sleep_for( std::chrono::seconds( m_nShutterDurationInSeconds ) );

	// Release shutter button
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