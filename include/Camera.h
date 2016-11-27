#pragma once

#include "Engine.h"

#include <thread>
#include <mutex>
#include <atomic>

#include "EDSDK.h"

class Camera : public ImageSource
{
    std::thread m_thCapture;
    std::mutex m_muCapture;
    std::list<cv::Mat> m_liCapturedImages;

    

    std::mutex m_muImageDownload;
    std::list<EdsDirectoryItemRef> m_liImagesToDownload;

    std::atomic_bool m_abCapture;

    std::vector<uint16_t> m_vBayerDataBuffer;
    EdsCameraRef m_CamRef;

public:
    Camera();
    ~Camera();

    ImageSource::Status GetStatus() const override;
    cv::Mat GetNextImage() override;
    void Initialize() override;
    void Finalize() override;

private:

    void threadProc();
    void handleShutDown();

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
};