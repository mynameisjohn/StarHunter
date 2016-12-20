#pragma once

#include <string>
#include <vector>

class TelescopeComm
{
	int m_nSlewRateX;
	int m_nSlewRateY;
	std::string m_strDeviceName;

#ifdef WIN32
	void * m_SerialPort;
#else
	int m_SerialPort;
#endif

	bool openPort();
	bool closePort();
	bool writeToPort( const char * pData, const size_t uDataSize ) const;
	std::vector<char> readPort( const size_t uDataSize ) const;
	std::vector<char> executeCommand( std::vector<char> vCMD ) const;

public:
	TelescopeComm( std::string strDeviceName );
	~TelescopeComm();

	void SetSlew( int nSlewRateX, int nSlewRateY );
	void GetSlew( int * pnSlewRateX, int * pnSlewRateY ) const;
	void GetMountPos( int * pnMountPosX, int * pnMountPosY ) const;
};