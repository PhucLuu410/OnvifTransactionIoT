#pragma once

#include "Camera/Interfaces/ICameraService.hpp"
#include "Camera/Streaming/GstPipeline.hpp"

namespace camera
{

    class GenericRtspCamera : public ICameraService
    {
    public:
        GenericRtspCamera();

        Status initialize(
            const CameraConfig &config) override;

        Status connect() override;

        Status startStream() override;

        Status stopStream() override;

        Status disconnect() override;

        CameraState getState() const override;

        void setFrameCallback(FrameCallback callback) override;

        CameraStatistics getStatistics() const override;

    private:
        CameraConfig config_;

        CameraState state_ = CameraState::DISCONNECTED;

        FrameCallback frame_callback_;

        GstPipeline pipeline_;
    };

}