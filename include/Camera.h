#pragma once

#if SH_CAMERA

#include "Engine.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

#ifdef WIN32
#include "EDSDK.h"
#else
#include <gphoto2/gphoto2.h>
#endif

class SHCamera : public ImageSource
{
    std::thread m_thCapture;
    std::mutex m_muCapture;
    std::list<img_t> m_liCapturedImages;

#ifdef WIN32
    std::mutex m_muCamSync;
    std::condition_variable m_cvCamSync;
    EdsDirectoryItemRef m_ImgRefToDownload;
    EdsCameraRef m_CamRef;
#else
    GPContext * m_pGPContext;
    Camera * m_pGPCamera;
#endif

    std::atomic_bool m_abCapture;
    std::vector<uint16_t> m_vBayerDataBuffer;
public:
    SHCamera();
    ~SHCamera();

    ImageSource::Status GetStatus() const override;
    img_t GetNextImage() override;
    void Initialize() override;
    void Finalize() override;

private:

    void threadProc();

#ifdef WIN32
    EdsError handleObjectEvent_impl( EdsUInt32			inEvent,
                                     EdsBaseRef			inRef,
                                     EdsVoid *			inContext );
    EdsError handleStateEvent_impl(
        EdsUInt32			inEvent,
        EdsUInt32			inParam,
        EdsVoid *			inContext
    );

    static EdsError EDSCALLBACK handleObjectEvent(
        EdsUInt32			inEvent,
        EdsBaseRef			inRef,
        EdsVoid *			inContext
    );

    static EdsError EDSCALLBACK  handleStateEvent(
        EdsUInt32			inEvent,
        EdsUInt32			inParam,
        EdsVoid *			inContext
    );
#endif
};

#endif