#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace camera
{

    enum class CameraState
    {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        STREAMING,
        ERROR
    };

    enum class CameraError
    {
        NONE,
        INVALID_CONFIG,
        CONNECTION_FAILED,
        STREAM_FAILED,
        PIPELINE_ERROR
    };

    struct Status
    {
        bool success = false;
        CameraError error = CameraError::NONE;
        std::string message;
    };

    struct CameraConfig
    {
        std::string id;
        std::string rtsp_url;
        int latency_ms = 200;
    };

    struct FrameData
    {
        std::vector<uint8_t> data;

        int width = 0;
        int height = 0;
        int stride = 0;

        std::string format;
        uint64_t timestamp_ns = 0;
    };

    using FramePtr = std::shared_ptr<const FrameData>;

    struct CameraStatistics
    {
        double fps = 0.0;
        uint64_t frames_received = 0;
        uint64_t frames_dropped = 0;
    };

}