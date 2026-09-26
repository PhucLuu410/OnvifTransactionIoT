#ifndef NETWORK_UDP_SOCKET_H
#define NETWORK_UDP_SOCKET_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

namespace Eyerus
{

    class UdpSocket
    {
    public:
        UdpSocket();
        ~UdpSocket();

        UdpSocket(const UdpSocket &) = delete;
        UdpSocket &operator=(const UdpSocket &) = delete;

        // Gắn socket vào IP card mạng và port (port 0: OS tự cấp port)
        bool bindTo(const std::string &localIp = "", int localPort = 0);

        // Gửi dữ liệu UDP
        bool sendTo(const std::string &data, const std::string &destIp, int destPort);

        // Nhận dữ liệu UDP kèm timeout (milliseconds)
        bool receiveFrom(std::string &outData, std::string &outSenderIp, int &outSenderPort, int timeoutMs);

        void close();

        bool isValid() const;

    private:
        SOCKET m_socket = INVALID_SOCKET;
    };

} // namespace Eyerus

#endif // NETWORK_UDP_SOCKET_H
