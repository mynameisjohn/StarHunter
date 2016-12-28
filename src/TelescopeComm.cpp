#if SH_TELESCOPE

#include "TelescopeComm.h"

#include <pyliaison.h>

#include <iostream>

// Implementation is just a light wrapper
// around a pyl::Object (is this necessary?)
struct _TelescopeComm_impl
{
    pyl::Object obTelescope;
};

TelescopeComm::TelescopeComm(std::string strDevice)
{
    try
    {
        // Init interpreter
        // TODO Don't re-initialize - add a flag / refcount
        pyl::initialize();

        // Get the TelescopeComm module
        auto obTelMod = pyl::Object::from_script("TelescopeComm.py");

        // Construct TelescopeComm python object from
        // factory function, store in impl member
        m_pImpl.reset(new _TelescopeComm_impl);
        m_pImpl->obTelescope = obTelMod.call(
                "TelescopeComm.Factory", strDevice);
    }
    // An error will be thrown if something went wrong
    catch (pyl::runtime_error e)
    {
        // Print error, close interpreter
        std::cout << e.what() << std::endl;
        pyl::finalize();

        // Pass it along to the client
        throw std::runtime_error(e.what());
    }
}

TelescopeComm::~TelescopeComm()
{
    // Same thing here - maybe a ref count?
    pyl::finalize();
}

// Call slewVariable twice on the implementation
// for the alt and azm directions (NOP if no change)
void TelescopeComm::SetSlewRate(int alt, int azm)
{
    m_pImpl->obTelescope.call("slewVariable", "Alt", alt);
    m_pImpl->obTelescope.call("slewVariable", "Azm", azm);
/*    
    bool bSuccess(false);
    if (!m_pImpl->obTelescope.call("slewVariable", "Alt", alt))
        throw std::runtime_error("Error setting Alt Speed");
    if (!m_pImpl->obTelescope.call("slewVariable", "Azm", azm))
        throw std::runtime_error("Error setting Azm Speed");
*/
}

// Get member variables of the python TelescopeComm object
void TelescopeComm::GetSlewRate(int * pAlt, int * pAzm)
{
    // Get values for pointers provided
    int nAlt(0), nAzm(0);
    if (pAlt)
    {
        if (!m_pImpl->obTelescope.get_attr("nAltSpeed", nAlt))
            throw std::runtime_error("Error getting Alt Speed");
        *pAlt = nAlt;
    }
    if (pAzm)
    {
        if (!m_pImpl->obTelescope.get_attr("nAzmSpeed", nAzm))
            throw std::runtime_error("Error getting Alt Speed");
        *pAzm = nAzm;
    }
}

// Invoke GetPosition on the python object
void TelescopeComm::GetMountPos(int * pAlt, int * pAzm)
{
    // GetPosition returns a list of size 2, which we convert to an array
    std::array<int, 2> aResp;

    // Convert to the array - it better work!
    if (!m_pImpl->obTelescope.call("GetPosition").convert(aResp))
        throw std::runtime_error("Error getting mount position!");

    // Give them what they want
    if (pAlt)
        *pAlt = aResp[0];
    if (pAzm)
        *pAzm = aResp[1];
}

#endif // SH_TELESCOPE
