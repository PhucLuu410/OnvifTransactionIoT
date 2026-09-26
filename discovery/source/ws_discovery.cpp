#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include "ws_discovery.h"
#include "net_utils.h"
#include "udp_socket.h"
#include "pugixml.hpp"
#include <sstream>
#include <iostream>
#include <chrono>
#include <map>
#include <memory>
#include <random>
#include <iomanip>

namespace Eyerus
{

    namespace
    {

        std::string generateSimpleUuid()
        {
            static std::random_device rd;
            static std::mt19937_64 gen(rd());
            static std::uniform_int_distribution<uint64_t> dis;

            uint64_t p1 = dis(gen);
            uint64_t p2 = dis(gen);

            p1 = (p1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
            p2 = (p2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            ss << std::setw(8) << (uint32_t)(p1 >> 32) << "-"
               << std::setw(4) << (uint16_t)((p1 >> 16) & 0xFFFF) << "-"
               << std::setw(4) << (uint16_t)(p1 & 0xFFFF) << "-"
               << std::setw(4) << (uint16_t)(p2 >> 48) << "-"
               << std::setw(12) << (uint64_t)(p2 & 0xFFFFFFFFFFFFULL);

            return ss.str();
        }

        bool localNameEquals(const char *nodeName, const std::string &targetName)
        {
            if (!nodeName)
                return false;
            std::string name(nodeName);
            size_t colon = name.find(':');
            std::string local = (colon != std::string::npos) ? name.substr(colon + 1) : name;
            return (local == targetName);
        }

        pugi::xml_node findChildByLocalName(pugi::xml_node parent, const std::string &localName)
        {
            for (pugi::xml_node child = parent.first_child(); child; child = child.next_sibling())
            {
                if (localNameEquals(child.name(), localName))
                {
                    return child;
                }
            }
            return pugi::xml_node();
        }

        void findNodesByLocalName(pugi::xml_node root, const std::string &targetName, std::vector<pugi::xml_node> &outList)
        {
            if (localNameEquals(root.name(), targetName))
            {
                outList.push_back(root);
            }
            for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling())
            {
                findNodesByLocalName(child, targetName, outList);
            }
        }

        std::vector<std::string> splitTokens(const std::string &input)
        {
            std::vector<std::string> tokens;
            std::istringstream iss(input);
            std::string token;
            while (iss >> token)
            {
                tokens.push_back(token);
            }
            return tokens;
        }

    } // anonymous namespace

    std::string WsDiscovery::createProbeMessage(const std::string &customUuid, const std::string &typeFilter)
    {
        std::string messageId = customUuid.empty() ? generateSimpleUuid() : customUuid;

        std::ostringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
           << "<e:Envelope "
           << "xmlns:e=\"http://www.w3.org/2003/05/soap-envelope\" "
           << "xmlns:w=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
           << "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
           << "xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" "
           << "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
           << "  <e:Header>\r\n"
           << "    <w:MessageID>uuid:" << messageId << "</w:MessageID>\r\n"
           << "    <w:To e:mustUnderstand=\"true\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</w:To>\r\n"
           << "    <w:Action w:mustUnderstand=\"true\">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</w:Action>\r\n"
           << "  </e:Header>\r\n"
           << "  <e:Body>\r\n"
           << "    <d:Probe>\r\n";

        if (!typeFilter.empty())
        {
            ss << "      <d:Types>" << typeFilter << "</d:Types>\r\n";
        }

        ss << "    </d:Probe>\r\n"
           << "  </e:Body>\r\n"
           << "</e:Envelope>";

        return ss.str();
    }

    std::vector<CameraDevice> WsDiscovery::parseProbeMatch(const char *xmlBuffer, size_t length, const std::string &senderIp)
    {
        std::vector<CameraDevice> devices;
        if (!xmlBuffer || length == 0)
        {
            return devices;
        }

        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_buffer(xmlBuffer, length);
        if (!result)
        {
            return devices;
        }

        std::vector<pugi::xml_node> probeMatches;
        findNodesByLocalName(doc, "ProbeMatch", probeMatches);

        for (const auto &matchNode : probeMatches)
        {
            CameraDevice device;
            device.ip = senderIp;

            // 1. EndpointReference -> Address
            pugi::xml_node epNode = findChildByLocalName(matchNode, "EndpointReference");
            if (epNode)
            {
                pugi::xml_node addrNode = findChildByLocalName(epNode, "Address");
                if (addrNode)
                {
                    device.endpointReference = addrNode.child_value();
                }
            }

            // 2. Types
            pugi::xml_node typesNode = findChildByLocalName(matchNode, "Types");
            if (typesNode)
            {
                device.types = splitTokens(typesNode.child_value());
            }

            // 3. Scopes
            pugi::xml_node scopesNode = findChildByLocalName(matchNode, "Scopes");
            if (scopesNode)
            {
                device.scopes = splitTokens(scopesNode.child_value());
                device.parseScopes();
            }

            // 4. XAddrs
            pugi::xml_node xaddrsNode = findChildByLocalName(matchNode, "XAddrs");
            if (xaddrsNode)
            {
                device.xaddrs = splitTokens(xaddrsNode.child_value());

                if (!device.xaddrs.empty())
                {
                    std::string host, path;
                    int port = 80;
                    if (NetUtils::parseUrl(device.xaddrs[0], host, port, path))
                    {
                        device.ip = host;
                        device.port = port;
                    }
                }
            }

            devices.push_back(device);
        }

        return devices;
    }

    std::vector<CameraDevice> WsDiscovery::scanNetwork(
        int timeoutSeconds,
        const std::string &specificInterfaceIp,
        OnDeviceFoundCallback onDeviceFound,
        OnProgressCallback onProgress)
    {
        std::vector<CameraDevice> resultDevices;

        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            if (onProgress)
                onProgress("Loi: Khoi tao Winsock that bai.");
            return resultDevices;
        }

        std::vector<NetworkInterface> interfaces;
        if (!specificInterfaceIp.empty())
        {
            NetworkInterface iface;
            iface.ipAddress = specificInterfaceIp;
            iface.name = "Custom";
            iface.description = "Card mang chi dinh";
            interfaces.push_back(iface);
        }
        else
        {
            interfaces = NetUtils::getActiveIPv4Interfaces();
            if (interfaces.empty())
            {
                NetworkInterface iface;
                iface.ipAddress = "0.0.0.0";
                iface.name = "Default";
                iface.description = "Card mang mac dinh";
                interfaces.push_back(iface);
            }
        }

        struct Session
        {
            NetworkInterface iface;
            std::unique_ptr<UdpSocket> socket;
        };

        std::vector<Session> sessions;
        std::string probeXml = createProbeMessage();

        for (const auto &iface : interfaces)
        {
            auto sock = std::unique_ptr<UdpSocket>(new UdpSocket());
            if (sock->bindTo(iface.ipAddress, 0))
            {
                if (onProgress)
                {
                    onProgress("Phat Probe WS-Discovery tren " + iface.ipAddress + " (" + iface.description + ")...");
                }
                sock->sendTo(probeXml, "239.255.255.250", 3702);
                sessions.push_back(Session{iface, std::move(sock)});
            }
        }

        if (sessions.empty())
        {
            if (onProgress)
                onProgress("Loi: Khong the mo socket UDP tren bat ky card mang nao.");
            WSACleanup();
            return resultDevices;
        }

        if (onProgress)
        {
            onProgress("Dang lang nghe phan hoi trong " + std::to_string(timeoutSeconds) + " giay...");
        }

        std::map<std::string, CameraDevice> uniqueMap;
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + std::chrono::seconds(timeoutSeconds);

        while (std::chrono::steady_clock::now() < endTime)
        {
            for (auto &session : sessions)
            {
                std::string buffer, senderIp;
                int senderPort = 0;

                if (session.socket->receiveFrom(buffer, senderIp, senderPort, 120))
                {
                    auto found = parseProbeMatch(buffer.c_str(), buffer.length(), senderIp);
                    for (auto &dev : found)
                    {
                        std::string key = dev.getUniqueKey();
                        if (uniqueMap.find(key) == uniqueMap.end())
                        {
                            uniqueMap[key] = dev;
                            if (onDeviceFound)
                            {
                                onDeviceFound(dev);
                            }
                        }
                    }
                }
            }
        }

        for (const auto &pair : uniqueMap)
        {
            resultDevices.push_back(pair.second);
        }

        if (onProgress)
        {
            onProgress("Hoan tat quet mang. Tim thay " + std::to_string(resultDevices.size()) + " thiet bi.");
        }

        WSACleanup();
        return resultDevices;
    }

} // namespace Eyerus
