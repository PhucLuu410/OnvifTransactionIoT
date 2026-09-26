#ifndef NETWORK_NET_UTILS_H
#define NETWORK_NET_UTILS_H

#include <string>
#include <vector>

namespace Eyerus
{

    struct NetworkInterface
    {
        std::string name;
        std::string description;
        std::string ipAddress;
    };

    class NetUtils
    {
    public:
        // Liệt kê tất cả các card mạng IPv4 đang hoạt động (Wi-Fi, Ethernet,...)
        static std::vector<NetworkInterface> getActiveIPv4Interfaces();

        // Tách URL (vd: http://192.168.1.200:8000/onvif/device_service) thành Host, Port, Path
        static bool parseUrl(const std::string &url, std::string &outHost, int &outPort, std::string &outPath);
    };

} // namespace Eyerus

#endif // NETWORK_NET_UTILS_H
