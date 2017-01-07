#pragma once

#if SH_TELESCOPE

// Forward declare impl
// this is implemented as a python class
class _TelescopeComm_impl;

#include <memory>
#include <string>

// Minimal interface to python class
class TelescopeComm
{
    std::unique_ptr<_TelescopeComm_impl> m_pImpl;
	std::string m_strDeviceName;
public:

    TelescopeComm(std::string strDevice);
    ~TelescopeComm();

	void Initialize();

    void SetSlewRate(int alt, int azm);
    void GetSlewRate(int * pAlt, int * pAzm);
    void GetMountPos(int * pAlt, int * pAzm);
};

#endif // SH_TELESCOPE
