#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include "http_client.h"
#include "net_utils.h"
#include <chrono>
#include <sstream>
#include <vector>

namespace Eyerus
{

    bool HttpClient::postSoap(
        const std::string &url,
        const std::string &soapBody,
        HttpResponse &outResponse,
        int timeoutMs)
    {
        std::string host, path;
        int port = 80;

        if (!NetUtils::parseUrl(url, host, port, path))
        {
            return false;
        }

        auto startClock = std::chrono::steady_clock::now();

        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET)
        {
            return false;
        }

        DWORD timeout = timeoutMs;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));

        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(static_cast<u_short>(port));

        if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) != 1)
        {
            addrinfo hints = {}, *res = nullptr;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res)
            {
                closesocket(sock);
                return false;
            }
            serverAddr = *(sockaddr_in *)res->ai_addr;
            freeaddrinfo(res);
        }

        if (connect(sock, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            closesocket(sock);
            return false;
        }

        std::ostringstream request;
        request << "POST " << path << " HTTP/1.1\r\n"
                << "Host: " << host << ":" << port << "\r\n"
                << "Content-Type: application/soap+xml; charset=utf-8\r\n"
                << "Content-Length: " << soapBody.length() << "\r\n"
                << "Connection: close\r\n\r\n"
                << soapBody;

        std::string reqStr = request.str();
        int sent = send(sock, reqStr.c_str(), static_cast<int>(reqStr.length()), 0);
        if (sent == SOCKET_ERROR)
        {
            closesocket(sock);
            return false;
        }

        std::vector<char> buffer(4096);
        std::string fullResponse;
        int bytesReceived = 0;
        while ((bytesReceived = recv(sock, buffer.data(), static_cast<int>(buffer.size()) - 1, 0)) > 0)
        {
            fullResponse.append(buffer.data(), bytesReceived);
        }

        closesocket(sock);

        auto endClock = std::chrono::steady_clock::now();
        outResponse.roundTripMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(endClock - startClock).count());

        if (fullResponse.empty())
        {
            return false;
        }

        // Tách HTTP Header và Body
        size_t headerEnd = fullResponse.find("\r\n\r\n");
        if (headerEnd != std::string::npos)
        {
            outResponse.header = fullResponse.substr(0, headerEnd);
            outResponse.body = fullResponse.substr(headerEnd + 4);
        }
        else
        {
            outResponse.body = fullResponse;
        }

        // Parse HTTP Status code (vd: HTTP/1.1 200 OK)
        outResponse.statusCode = 0;
        size_t space1 = fullResponse.find(' ');
        if (space1 != std::string::npos && space1 + 4 <= fullResponse.length())
        {
            try
            {
                outResponse.statusCode = std::stoi(fullResponse.substr(space1 + 1, 3));
            }
            catch (...)
            {
                outResponse.statusCode = 0;
            }
        }

        return true;
    }

} // namespace Eyerus
