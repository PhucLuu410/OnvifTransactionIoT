#ifndef DISCOVERY_WS_DISCOVERY_H
#define DISCOVERY_WS_DISCOVERY_H

#include "camera_device.h"
#include <string>
#include <vector>
#include <functional>

namespace Eyerus
{

    using OnDeviceFoundCallback = std::function<void(const CameraDevice &)>;
    using OnProgressCallback = std::function<void(const std::string &statusMessage)>;

    class WsDiscovery
    {
    public:
        // Tạo bản tin XML WS-Discovery Probe
        static std::string createProbeMessage(
            const std::string &customUuid = "",
            const std::string &typeFilter = "dn:NetworkVideoTransmitter tds:Device");

        // Phân tích XML phản hồi ProbeMatch từ RAM
        static std::vector<CameraDevice> parseProbeMatch(
            const char *xmlBuffer,
            size_t length,
            const std::string &senderIp);

        // Dò quét mạng toàn diện để tìm tất cả các camera ONVIF
        static std::vector<CameraDevice> scanNetwork(
            int timeoutSeconds = 3,
            const std::string &specificInterfaceIp = "",
            OnDeviceFoundCallback onDeviceFound = nullptr,
            OnProgressCallback onProgress = nullptr);
    };

} // namespace Eyerus

#endif // DISCOVERY_WS_DISCOVERY_H
