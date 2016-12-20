#include "TelescopeComm.h"

#include <iostream>
#include <sstream>

#ifdef WIN32
#include <Windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#else
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#endif


TelescopeComm::TelescopeComm( std::string strDeviceName ) :
	m_nSlewRateX( 0 ),
	m_nSlewRateY( 0 ),
	m_strDeviceName( strDeviceName ),
	m_SerialPort( 0 )
{
	openPort();
}

TelescopeComm::~TelescopeComm()
{
	try
	{
		closePort();
	}
	catch ( std::runtime_error e )
	{
	}
}

bool TelescopeComm::openPort()
{
	if ( m_SerialPort )
		return true;

	std::string strErrMsg = "Error! Unable to open serial port " + m_strDeviceName;

#ifdef WIN32
	m_SerialPort = ::CreateFile( m_strDeviceName.c_str(),
								 GENERIC_READ | GENERIC_WRITE,  // access ( read and write)
								 0,                           // (share) 0:cannot share the
															  // COM port
								 0,                           // security  (None)
								 OPEN_EXISTING,               // creation : open_existing
								 FILE_FLAG_OVERLAPPED,        // we want overlapped operation
								 0                            // no templates file for
															  // COM port...
	);
	if ( m_SerialPort == nullptr )
		throw std::runtime_error( strErrMsg );
#else
	m_SerialPort = open( m_strDeviceName.c_str(), O_RDWR | O_NOCTTY );
	if ( m_SerialPort < 0 )
		throw std::runtime_error( strErrMsg );
#endif

	return ( m_SerialPort != 0 );
}

bool TelescopeComm::closePort()
{
#ifdef WIN32
	if ( m_SerialPort == nullptr )
		return true;

	if ( !::CloseHandle( m_SerialPort ) )
	{
		throw std::runtime_error( "Error: Unable to close serial port!" );
		return false;
	}
#else
	if ( m_SerialPort <= 0 )
		return true;

	if ( close( m_SerialPort ) < 0 )
	{
		throw std::runtime_error( "Error: Unable to close serial port!" );
		return false;
	}
#endif

	m_SerialPort = 0;
	return true;
}

bool TelescopeComm::writeToPort( const char * pData, const size_t uDataSize ) const
{
#ifdef WIN32
	DWORD dwWritten( 0 );
	OVERLAPPED sOverlap { 0 };
	if ( !::WriteFile( m_SerialPort, pData, uDataSize, &dwWritten, &sOverlap ) )
		throw std::runtime_error( "Error: Unable to write to serial port!" );
	else if ( dwWritten != (DWORD) uDataSize )
		throw std::runtime_error( "Error: Invalid # of bytes written to serial port!" );
#else
	int nWritten = write( m_SerialPort, pData, uDataSize );
	if ( nWritten < 0 )
		throw std::runtime_error( "Error: Unable to write to serial port!" );
	else if ( nWritten < (int) uDataSize )
		throw std::runtime_error( "Error: Invalid # of bytes written to serial port!" );
#endif 
	return true;
}

std::vector<char> TelescopeComm::readPort( const size_t uDataSize ) const
{
	std::vector<char> vRet( uDataSize );
#ifdef WIN32
	DWORD dwBytesRead( 0 );
	OVERLAPPED ovRead { 0 };
	if ( !::ReadFile( m_SerialPort, vRet.data(), uDataSize, &dwBytesRead, &ovRead ) )
		throw std::runtime_error( "Error: Unable to read from serial port!" );
	else if ( dwBytesRead != uDataSize )
		throw std::runtime_error( "Error: Invalid # of bytes read!" );
#else
	int nRead = read( m_SerialPort, vRet.data(), uDataSize );
	if ( nRead < 0 )
		throw std::runtime_error( "Error: Unable to read from serial port!" );
	else if ( nRead < (int) uDataSize )
		throw std::runtime_error( "Error: Invalid # of bytes read!" );
#endif
	return vRet;
}

std::vector<char> TelescopeComm::executeCommand( std::vector<char> vCMD ) const
{
	if ( writeToPort( vCMD.data(), sizeof( char ) * vCMD.size() ) )
	{
		// response is one '#' character
		std::vector<char> vResponse = readPort( 1 );
		if ( vResponse.empty() )
			throw std::runtime_error( "Error: No response from telescope!" );
		else if ( vResponse.back() != '#' )
			throw std::runtime_error( "Error: Stop byte not recieved from telescope!" );
		return vResponse;
	}
	else
		throw std::runtime_error( "Error: Unable to write data to telescope!" );

	return {};
}

std::vector<char> makeVariableSlewRateCMD( int nSlewRate, bool bAlt )
{
	char nSlewRate_high = (char) ( ( 4 * nSlewRate ) / 256 );
	char nSlewRate_low = (char) ( ( 4 * nSlewRate ) % 256 );
	return { 'P', 3, char( bAlt ? 16 : 17 ), char( nSlewRate > 0 ? 6 : 7 ), nSlewRate_high, nSlewRate_low, 0, 0 };
}

void TelescopeComm::SetSlew( int nSlewRateX, int nSlewRateY )
{
	executeCommand( makeVariableSlewRateCMD( nSlewRateX, true ) );
	m_nSlewRateX = nSlewRateX;

	executeCommand( makeVariableSlewRateCMD( nSlewRateY, false ) );
	m_nSlewRateY = nSlewRateY;
}

void TelescopeComm::GetSlew( int * pnSlewRateX, int * pnSlewRateY ) const
{
	if ( pnSlewRateX )
		*pnSlewRateX = m_nSlewRateX;
	if ( pnSlewRateY )
		*pnSlewRateY = m_nSlewRateY;
}

void TelescopeComm::GetMountPos( int * pnMountPosX, int * pnMountPosY ) const
{
	std::vector<char> vResp = executeCommand( { 'Z' } );
	if ( vResp.size() > 1 )
	{
		std::string strResp( vResp.begin(), vResp.end() - 1 );

		size_t ixComma = strResp.find( ',' );
		std::string strAzm( strResp.begin(), strResp.end() + ixComma );
		std::string strAlt( strResp.begin() + ixComma + 1, strResp.end() );

		int nMountPosX( 0 ), nMountPosY( 0 );
		std::istringstream( strAzm ) >> std::hex >> nMountPosX;
		std::istringstream( strAlt ) >> std::hex >> nMountPosY;

		if ( pnMountPosX )
			*pnMountPosX = nMountPosX;
		if ( pnMountPosY )
			*pnMountPosY = nMountPosY;
	}
}