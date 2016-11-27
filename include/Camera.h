#pragma once

#include "Engine.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

#ifdef WIN32
#include "EDSDK.h"
#endif

class Camera : public ImageSource
{
    std::thread m_thCapture;
    std::mutex m_muCapture;
    std::list<cv::Mat> m_liCapturedImages;

#ifdef WIN32
    std::mutex m_muCamSync;
    std::condition_variable m_cvCamSync;
    EdsDirectoryItemRef m_ImgRefToDownload;
    EdsCameraRef m_CamRef;
#endif

    std::atomic_bool m_abCapture;
    std::vector<uint16_t> m_vBayerDataBuffer;
public:
    Camera();
    ~Camera();

    ImageSource::Status GetStatus() const override;
    cv::Mat GetNextImage() override;
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