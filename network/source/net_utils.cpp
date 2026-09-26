#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include "net_utils.h"
#include <cstdlib>

namespace Eyerus
{

    std::vector<NetworkInterface> NetUtils::getActiveIPv4Interfaces()
    {
        std::vector<NetworkInterface> interfaces;

        ULONG outBufLen = 15000;
        PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
        ULONG iterations = 0;
        DWORD dwRetVal = 0;

        do
        {
            pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
            if (pAddresses == nullptr)
            {
                return interfaces;
            }

            dwRetVal = GetAdaptersAddresses(
                AF_INET,
                GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                nullptr,
                pAddresses,
                &outBufLen);

            if (dwRetVal == ERROR_BUFFER_OVERFLOW)
            {
                free(pAddresses);
                pAddresses = nullptr;
            }
            else
            {
                break;
            }

            iterations++;
        } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (iterations < 3));

        if (dwRetVal == NO_ERROR && pAddresses != nullptr)
        {
            for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses != nullptr; pCurrAddresses = pCurrAddresses->Next)
            {
                if (pCurrAddresses->OperStatus != IfOperStatusUp)
                    continue;
                if (pCurrAddresses->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                    continue;

                for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != nullptr; pUnicast = pUnicast->Next)
                {
                    if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
                    {
                        sockaddr_in *sa_in = (sockaddr_in *)pUnicast->Address.lpSockaddr;
                        char ipStr[INET_ADDRSTRLEN] = {0};
                        inet_ntop(AF_INET, &(sa_in->sin_addr), ipStr, sizeof(ipStr));

                        std::string ip = ipStr;
                        if (!ip.empty() && ip != "127.0.0.1" && ip != "0.0.0.0")
                        {
                            NetworkInterface iface;
                            if (pCurrAddresses->AdapterName)
                            {
                                iface.name = pCurrAddresses->AdapterName;
                            }
                            if (pCurrAddresses->Description)
                            {
                                int size_needed = WideCharToMultiByte(CP_UTF8, 0, pCurrAddresses->Description, -1, NULL, 0, NULL, NULL);
                                if (size_needed > 0)
                                {
                                    std::string desc(size_needed - 1, 0);
                                    WideCharToMultiByte(CP_UTF8, 0, pCurrAddresses->Description, -1, &desc[0], size_needed, NULL, NULL);
                                    iface.description = desc;
                                }
                            }
                            iface.ipAddress = ip;
                            interfaces.push_back(iface);
                        }
                    }
                }
            }
        }

        if (pAddresses)
        {
            free(pAddresses);
        }

        return interfaces;
    }

    bool NetUtils::parseUrl(const std::string &url, std::string &outHost, int &outPort, std::string &outPath)
    {
        size_t protocolPos = url.find("://");
        std::string remaining;
        bool isHttps = false;

        if (protocolPos != std::string::npos)
        {
            std::string proto = url.substr(0, protocolPos);
            if (proto == "https")
                isHttps = true;
            remaining = url.substr(protocolPos + 3);
        }
        else
        {
            remaining = url;
        }

        outPort = isHttps ? 443 : 80;

        size_t slashPos = remaining.find('/');
        std::string hostPort;
        if (slashPos != std::string::npos)
        {
            hostPort = remaining.substr(0, slashPos);
            outPath = remaining.substr(slashPos);
        }
        else
        {
            hostPort = remaining;
            outPath = "/";
        }

        size_t colonPos = hostPort.find(':');
        if (colonPos != std::string::npos)
        {
            outHost = hostPort.substr(0, colonPos);
            try
            {
                outPort = std::stoi(hostPort.substr(colonPos + 1));
            }
            catch (...)
            {
                // Keep default port
            }
        }
        else
        {
            outHost = hostPort;
        }

        return !outHost.empty();
    }

} // namespace Eyerus
