#include "udp_socket.h"
#include <vector>

namespace Eyerus
{

    UdpSocket::UdpSocket() : m_socket(INVALID_SOCKET)
    {
    }

    UdpSocket::~UdpSocket()
    {
        close();
    }

    bool UdpSocket::bindTo(const std::string &localIp, int localPort)
    {
        close();

        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == INVALID_SOCKET)
        {
            return false;
        }

        int reuse = 1;
        setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

        int broadcast = 1;
        setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast, sizeof(broadcast));

        int ttl = 4;
        setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)&ttl, sizeof(ttl));

        sockaddr_in localAddr = {};
        localAddr.sin_family = AF_INET;
        localAddr.sin_port = htons(static_cast<u_short>(localPort));

        if (!localIp.empty() && localIp != "0.0.0.0")
        {
            inet_pton(AF_INET, localIp.c_str(), &localAddr.sin_addr);
        }
        else
        {
            localAddr.sin_addr.s_addr = INADDR_ANY;
        }

        if (bind(m_socket, (sockaddr *)&localAddr, sizeof(localAddr)) == SOCKET_ERROR)
        {
            close();
            return false;
        }

        if (!localIp.empty() && localIp != "0.0.0.0")
        {
            in_addr localInterface = {};
            if (inet_pton(AF_INET, localIp.c_str(), &localInterface) == 1)
            {
                setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&localInterface, sizeof(localInterface));
            }
        }

        return true;
    }

    bool UdpSocket::sendTo(const std::string &data, const std::string &destIp, int destPort)
    {
        if (m_socket == INVALID_SOCKET)
        {
            return false;
        }

        sockaddr_in destAddr = {};
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(static_cast<u_short>(destPort));
        if (inet_pton(AF_INET, destIp.c_str(), &destAddr.sin_addr) != 1)
        {
            return false;
        }

        int bytesSent = sendto(m_socket, data.c_str(), static_cast<int>(data.length()), 0, (sockaddr *)&destAddr, sizeof(destAddr));
        return (bytesSent != SOCKET_ERROR);
    }

    bool UdpSocket::receiveFrom(std::string &outData, std::string &outSenderIp, int &outSenderPort, int timeoutMs)
    {
        if (m_socket == INVALID_SOCKET)
        {
            return false;
        }

        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(m_socket, &readFds);

        timeval tv = {};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        int selResult = select(0, &readFds, nullptr, nullptr, &tv);
        if (selResult <= 0)
        {
            return false;
        }

        std::vector<char> buffer(16384);
        sockaddr_in senderAddr = {};
        int senderAddrLen = sizeof(senderAddr);

        int bytesReceived = recvfrom(m_socket, buffer.data(), static_cast<int>(buffer.size()) - 1, 0, (sockaddr *)&senderAddr, &senderAddrLen);
        if (bytesReceived <= 0)
        {
            return false;
        }

        buffer[bytesReceived] = '\0';
        outData.assign(buffer.data(), bytesReceived);

        char ipStr[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &(senderAddr.sin_addr), ipStr, sizeof(ipStr));
        outSenderIp = ipStr;
        outSenderPort = ntohs(senderAddr.sin_port);

        return true;
    }

    void UdpSocket::close()
    {
        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

    bool UdpSocket::isValid() const
    {
        return (m_socket != INVALID_SOCKET);
    }

} // namespace Eyerus
