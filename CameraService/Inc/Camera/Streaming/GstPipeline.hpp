
#pragma once

#include "Camera/Core/CameraTypes.hpp"

#include <functional>

namespace camera
{

    class GstPipeline
    {
    public:
        using FrameCallback = std::function<void(FramePtr)>;

        GstPipeline() = default;
        ~GstPipeline();

        Status create(const CameraConfig &config);

        Status start();

        Status stop();

        void setFrameCallback(FrameCallback callback);

        CameraStatistics getStatistics() const;

    private:
        FrameCallback frame_callback_;
    };

} // namespace camera
