#include "camera_device.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

namespace Eyerus
{

    std::string CameraDevice::urlDecode(const std::string &str)
    {
        std::string result;
        result.reserve(str.length());
        for (size_t i = 0; i < str.length(); ++i)
        {
            if (str[i] == '%' && i + 2 < str.length())
            {
                std::string hex = str.substr(i + 1, 2);
                char ch = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
                if (ch != 0)
                {
                    result += ch;
                    i += 2;
                    continue;
                }
            }
            else if (str[i] == '+')
            {
                result += ' ';
            }
            else
            {
                result += str[i];
            }
        }
        return result;
    }

    void CameraDevice::parseScopes()
    {
        for (const auto &scope : scopes)
        {
            std::string lowerScope = scope;
            std::transform(lowerScope.begin(), lowerScope.end(), lowerScope.begin(), ::tolower);

            const std::string namePrefix = "/name/";
            const std::string hwPrefix = "/hardware/";
            const std::string locPrefix = "/location/";

            size_t pos = lowerScope.find(namePrefix);
            if (pos != std::string::npos && name.empty())
            {
                name = urlDecode(scope.substr(pos + namePrefix.length()));
                continue;
            }

            pos = lowerScope.find(hwPrefix);
            if (pos != std::string::npos && hardware.empty())
            {
                hardware = urlDecode(scope.substr(pos + hwPrefix.length()));
                continue;
            }

            pos = lowerScope.find(locPrefix);
            if (pos != std::string::npos && location.empty())
            {
                location = urlDecode(scope.substr(pos + locPrefix.length()));
                continue;
            }
        }

        if (name.empty())
        {
            name = "ONVIF Camera";
        }
        if (hardware.empty())
        {
            hardware = "Generic";
        }
        if (location.empty())
        {
            location = "Default";
        }
    }

    std::string CameraDevice::getMainXAddr() const
    {
        if (!xaddrs.empty())
        {
            return xaddrs[0];
        }
        return "http://" + ip + ":" + std::to_string(port) + "/onvif/device_service";
    }

    std::string CameraDevice::getUniqueKey() const
    {
        if (!endpointReference.empty())
        {
            return endpointReference;
        }
        return ip + ":" + std::to_string(port);
    }

    void CameraDevice::printDetails() const
    {
        std::cout << "--------------------------------------------------\n";
        std::cout << " Ten camera  : " << name << "\n";
        std::cout << " Dia chi IP  : " << ip << ":" << port << "\n";
        std::cout << " Model/HW    : " << hardware << "\n";
        std::cout << " Vi tri      : " << location << "\n";
        std::cout << " Endpoint ID : " << endpointReference << "\n";
        std::cout << " ONVIF XAddr : " << getMainXAddr() << "\n";
        if (xaddrs.size() > 1)
        {
            std::cout << " Cac XAddr khac:\n";
            for (size_t i = 1; i < xaddrs.size(); ++i)
            {
                std::cout << "   - " << xaddrs[i] << "\n";
            }
        }
        std::cout << "--------------------------------------------------\n";
    }

} // namespace Eyerus
