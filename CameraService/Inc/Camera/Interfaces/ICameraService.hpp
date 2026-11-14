#pragma once

#include <functional>
#include "Camera/Core/CameraTypes.hpp"

namespace camera
{

    class ICameraService
    {
    public:
        using FrameCallback = std::function<void(FramePtr)>;

        virtual Status initialize(
            const CameraConfig &config) = 0;

        virtual Status connect() = 0;

        virtual Status startStream() = 0;

        virtual Status stopStream() = 0;

        virtual Status disconnect() = 0;

        virtual CameraState getState() const = 0;

        virtual void setFrameCallback(
            FrameCallback callback) = 0;

        virtual CameraStatistics getStatistics() const = 0;

        virtual ~ICameraService() = default;
    };

}