#pragma once

#ifdef WIN32

#include <memory>
#include <list>
#include <algorithm>
#include <mutex>
#include <thread>
#include <atomic>

#include <EDSDK.h>

class CameraModel
{
protected:
	EdsCameraRef _camera;

	//Count of UIlock
	int		_lockCount;

	// Model name
	EdsChar  _modelName[EDS_MAX_NAME];

	// Taking a picture parameter
	EdsUInt32 _AEMode;
	EdsUInt32 _Av;
	EdsUInt32 _Tv;
	EdsUInt32 _Iso;
	EdsUInt32 _MeteringMode;
	EdsUInt32 _ExposureCompensation;
	EdsUInt32 _ImageQuality;
	EdsUInt32 _AvailableShot;
	EdsUInt32 _evfMode;
	EdsUInt32 _evfOutputDevice;
	EdsUInt32 _evfDepthOfFieldPreview;
	EdsUInt32 _evfZoom;
	EdsPoint  _evfZoomPosition;
	EdsRect	  _evfZoomRect;
	EdsUInt32 _evfAFMode;

	EdsFocusInfo _focusInfo;

	// List of value in which taking a picture parameter can be set
	EdsPropertyDesc _AEModeDesc;
	EdsPropertyDesc _AvDesc;
	EdsPropertyDesc _TvDesc;
	EdsPropertyDesc _IsoDesc;
	EdsPropertyDesc _MeteringModeDesc;
	EdsPropertyDesc _ExposureCompensationDesc;
	EdsPropertyDesc _ImageQualityDesc;
	EdsPropertyDesc _evfAFModeDesc;

public:
	// Constructor
	CameraModel( EdsCameraRef camera ) : _lockCount( 0 ), _camera( camera )
	{
		memset( &_focusInfo, 0, sizeof( _focusInfo ) );
	}

	~CameraModel()
	{
		if ( _camera )
			EdsRelease( _camera );
	}

	//Acquisition of Camera Object
	EdsCameraRef getCameraObject() const { return _camera; }


	//Property
public:
	// Taking a picture parameter
	void setAEMode( EdsUInt32 value ) { _AEMode = value; }
	void setTv( EdsUInt32 value ) { _Tv = value; }
	void setAv( EdsUInt32 value ) { _Av = value; }
	void setIso( EdsUInt32 value ) { _Iso = value; }
	void setMeteringMode( EdsUInt32 value ) { _MeteringMode = value; }
	void setExposureCompensation( EdsUInt32 value ) { _ExposureCompensation = value; }
	void setImageQuality( EdsUInt32 value ) { _ImageQuality = value; }
	void setEvfMode( EdsUInt32 value ) { _evfMode = value; }
	void setEvfOutputDevice( EdsUInt32 value ) { _evfOutputDevice = value; }
	void setEvfDepthOfFieldPreview( EdsUInt32 value ) { _evfDepthOfFieldPreview = value; }
	void setEvfZoom( EdsUInt32 value ) { _evfZoom = value; }
	void setEvfZoomPosition( EdsPoint value ) { _evfZoomPosition = value; }
	void setEvfZoomRect( EdsRect value ) { _evfZoomRect = value; }
	void setModelName( EdsChar *modelName ) { strcpy_s( _modelName, modelName ); }
	void setEvfAFMode( EdsUInt32 value ) { _evfAFMode = value; }
	void setFocusInfo( EdsFocusInfo value ) { _focusInfo = value; }

	// Taking a picture parameter
	EdsUInt32 getAEMode() const { return _AEMode; }
	EdsUInt32 getTv() const { return _Tv; }
	EdsUInt32 getAv() const { return _Av; }
	EdsUInt32 getIso() const { return _Iso; }
	EdsUInt32 getMeteringMode() const { return _MeteringMode; }
	EdsUInt32 getExposureCompensation() const { return _ExposureCompensation; }
	EdsUInt32 getImageQuality() const { return _ImageQuality; }
	EdsUInt32 getEvfMode() const { return _evfMode; }
	EdsUInt32 getEvfOutputDevice() const { return _evfOutputDevice; }
	EdsUInt32 getEvfDepthOfFieldPreview() const { return _evfDepthOfFieldPreview; }
	EdsUInt32 getEvfZoom() const { return _evfZoom; }
	EdsPoint  getEvfZoomPosition() const { return _evfZoomPosition; }
	EdsRect	  getEvfZoomRect() const { return _evfZoomRect; }
	EdsUInt32 getEvfAFMode() const { return _evfAFMode; }
	EdsChar *getModelName() { return _modelName; }
	EdsFocusInfo getFocusInfo()const { return _focusInfo; }

	//List of value in which taking a picture parameter can be set
	EdsPropertyDesc getAEModeDesc() const { return _AEModeDesc; }
	EdsPropertyDesc getAvDesc() const { return _AvDesc; }
	EdsPropertyDesc getTvDesc()	const { return _TvDesc; }
	EdsPropertyDesc getIsoDesc()	const { return _IsoDesc; }
	EdsPropertyDesc getMeteringModeDesc()	const { return _MeteringModeDesc; }
	EdsPropertyDesc getExposureCompensationDesc()	const { return _ExposureCompensationDesc; }
	EdsPropertyDesc getImageQualityDesc()	const { return _ImageQualityDesc; }
	EdsPropertyDesc getEvfAFModeDesc()	const { return _evfAFModeDesc; }

	//List of value in which taking a picture parameter can be set
	void setAEModeDesc( const EdsPropertyDesc* desc ) { _AEModeDesc = *desc; }
	void setAvDesc( const EdsPropertyDesc* desc ) { _AvDesc = *desc; }
	void setTvDesc( const EdsPropertyDesc* desc ) { _TvDesc = *desc; }
	void setIsoDesc( const EdsPropertyDesc* desc ) { _IsoDesc = *desc; }
	void setMeteringModeDesc( const EdsPropertyDesc* desc ) { _MeteringModeDesc = *desc; }
	void setExposureCompensationDesc( const EdsPropertyDesc* desc ) { _ExposureCompensationDesc = *desc; }
	void setImageQualityDesc( const EdsPropertyDesc* desc ) { _ImageQualityDesc = *desc; }
	void setEvfAFModeDesc( const EdsPropertyDesc* desc ) { _evfAFModeDesc = *desc; }

public:
	//Setting of taking a picture parameter(UInt32)
	void setPropertyUInt32( EdsUInt32 propertyID, EdsUInt32 value )
	{
		switch ( propertyID )
		{
			case kEdsPropID_AEModeSelect:			setAEMode( value );					break;
			case kEdsPropID_Tv:						setTv( value );						break;
			case kEdsPropID_Av:						setAv( value );						break;
			case kEdsPropID_ISOSpeed:				setIso( value );						break;
			case kEdsPropID_MeteringMode:			setMeteringMode( value );				break;
			case kEdsPropID_ExposureCompensation:	setExposureCompensation( value );		break;
			case kEdsPropID_ImageQuality:			setImageQuality( value );				break;
			case kEdsPropID_Evf_Mode:				setEvfMode( value );					break;
			case kEdsPropID_Evf_OutputDevice:		setEvfOutputDevice( value );			break;
			case kEdsPropID_Evf_DepthOfFieldPreview:setEvfDepthOfFieldPreview( value );	break;
			case kEdsPropID_Evf_AFMode:				setEvfAFMode( value );				break;
		}
	}

	//Setting of taking a picture parameter(String)
	void setPropertyString( EdsUInt32 propertyID, EdsChar *str )
	{
		switch ( propertyID )
		{
			case kEdsPropID_ProductName:			setModelName( str );					break;
		}
	}

	void setProeprtyFocusInfo( EdsUInt32 propertyID, EdsFocusInfo info )
	{
		switch ( propertyID )
		{
			case kEdsPropID_FocusInfo:				setFocusInfo( info );				break;
		}
	}

	//Setting of value list that can set taking a picture parameter
	void setPropertyDesc( EdsUInt32 propertyID, const EdsPropertyDesc* desc )
	{
		switch ( propertyID )
		{
			case kEdsPropID_AEModeSelect:			setAEModeDesc( desc );				break;
			case kEdsPropID_Tv:						setTvDesc( desc );					break;
			case kEdsPropID_Av:						setAvDesc( desc );					break;
			case kEdsPropID_ISOSpeed:				setIsoDesc( desc );					break;
			case kEdsPropID_MeteringMode:			setMeteringModeDesc( desc );			break;
			case kEdsPropID_ExposureCompensation:	setExposureCompensationDesc( desc );	break;
			case kEdsPropID_ImageQuality:			setImageQualityDesc( desc );			break;
			case kEdsPropID_Evf_AFMode:				setEvfAFModeDesc( desc );				break;
		}
	}

	//Acquisition of value list that can set taking a picture parameter
	EdsPropertyDesc getPropertyDesc( EdsUInt32 propertyID )
	{
		EdsPropertyDesc desc = { 0 };
		switch ( propertyID )
		{
			case kEdsPropID_AEModeSelect:			desc = getAEModeDesc();					break;
			case kEdsPropID_Tv:						desc = getTvDesc();						break;
			case kEdsPropID_Av:						desc = getAvDesc();						break;
			case kEdsPropID_ISOSpeed:				desc = getIsoDesc();					break;
			case kEdsPropID_MeteringMode:			desc = getMeteringModeDesc();			break;
			case kEdsPropID_ExposureCompensation:	desc = getExposureCompensationDesc();	break;
			case kEdsPropID_ImageQuality:			desc = getImageQualityDesc();			break;
			case kEdsPropID_Evf_AFMode:				desc = getEvfAFModeDesc();				break;
		}
		return desc;
	}

	//Access to camera
public:

};

class Command
{
protected:
	CameraModel * _model;
public:
	Command( CameraModel * pModel ) :_model( pModel ) {}

	CameraModel* getCameraModel() { return _model; }

	// Execute command
	virtual bool execute() = 0;
};

using CmdPtr = std::unique_ptr<Command>;
class CompositeCommand : public Command
{
	std::list<CmdPtr> m_liCommands;

public:
	CompositeCommand( CameraModel * pModel, std::initializer_list<Command *> liCommands ) : Command( pModel )
	{
		for ( Command * pCMD : liCommands )
			if ( pCMD )
				m_liCommands.emplace( m_liCommands.end(), pCMD );
	}
	bool execute() override
	{
		for ( auto itCMD = m_liCommands.begin(); itCMD != m_liCommands.end(); )
		{
			if ( itCMD->get()->execute() )
				itCMD = m_liCommands.erase( itCMD );
			else
				return false;
		}

		return m_liCommands.empty();
	}
};

class CommandQueue
{
	std::mutex m_muCommandMutex;
	std::list<CmdPtr> m_liCommands;
	CmdPtr m_pCloseCommand;
public:
	CommandQueue();
	~CommandQueue();
	CmdPtr pop();
	void push_back( Command * pCMD );
	void clear( bool bClose = false );
	void waitTillCompletion();

	void SetCloseCommand( Command * pCMD );
};

class DownloadCommand : public Command
{
public:
	class Receiver
	{
	public:
		virtual bool handleCapturedImage( EdsDirectoryItemRef dirItem ) = 0;
	};
private:
	EdsDirectoryItemRef _directoryItem;
	Receiver * m_pReceiver;
public:
	DownloadCommand( CameraModel *model, EdsDirectoryItemRef dirItem, Receiver * pReceiver = nullptr );
	virtual ~DownloadCommand();
	bool execute() override;
};

class OpenSessionCommand : public Command
{
public:
	OpenSessionCommand( CameraModel *model );
	virtual bool execute();
};

class CloseSessionCommand : public Command
{
public:
	CloseSessionCommand( CameraModel *model );
	virtual bool execute();
};

class GetPropertyCommand : public Command
{
private:
	EdsPropertyID _propertyID;
public:
	GetPropertyCommand( CameraModel *model, EdsPropertyID propertyID );
	virtual bool execute();
private:
	EdsError getProperty( EdsPropertyID propertyID );
};

class GetPropertyDescCommand : public Command
{
public:
	GetPropertyDescCommand( CameraModel *model, EdsPropertyID propertyID );
	virtual bool execute();
private:
	EdsPropertyID _propertyID;
	EdsError getPropertyDesc( EdsPropertyID propertyID );
};

class TakePictureCommand : public Command
{
public:
	TakePictureCommand( CameraModel *model ) : Command( model ) {}
	bool execute() override;
};

class StartEvfCommand : public Command
{
public:
	StartEvfCommand( CameraModel *model ) : Command( model ) {}
	bool execute() override;
};

class EndEvfCommand : public Command
{
public:
	EndEvfCommand( CameraModel *model ) : Command( model ) {}
	bool execute() override;
};

class DownloadEvfCommand : public Command
{
public:
	struct Receiver
	{
		virtual bool handleEvfImage() = 0;
	} *m_pReceiver { nullptr };

	DownloadEvfCommand( CameraModel *model, Receiver * pReceiver = nullptr );
	bool execute() override;
};

class SleepCommand : public Command
{
	uint32_t m_uSleepDur;
public:
	SleepCommand( CameraModel *model, uint32_t uSleepDur );
	virtual bool execute();
};

#endif WIN32