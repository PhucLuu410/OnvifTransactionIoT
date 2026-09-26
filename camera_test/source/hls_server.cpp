#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include "hls_server.h"
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>

namespace Eyerus
{

    namespace
    {

        std::atomic<bool> g_serverRunning(false);
        std::thread g_serverThread;
        SOCKET g_listenSocket = INVALID_SOCKET;

        const char *INDEX_HTML =
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "  <meta charset=\"utf-8\">\n"
            "  <title>Eyerus HLS Live Camera Player</title>\n"
            "  <script src=\"https://cdn.jsdelivr.net/npm/hls.js@latest\"></script>\n"
            "  <style>\n"
            "    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #121212; color: #fff; text-align: center; padding: 20px; }\n"
            "    .card { background: #1e1e1e; max-width: 960px; margin: 0 auto; padding: 20px; border-radius: 12px; box-shadow: 0 8px 24px rgba(0,0,0,0.5); }\n"
            "    video { width: 100%; border-radius: 8px; background: #000; outline: none; }\n"
            "    .badge { display: inline-block; padding: 5px 12px; background: #e50914; border-radius: 4px; font-weight: bold; margin-bottom: 15px; font-size: 14px; }\n"
            "    a { color: #4da6ff; text-decoration: none; }\n"
            "    a:hover { text-decoration: underline; }\n"
            "  </style>\n"
            "</head>\n"
            "<body>\n"
            "  <div class=\"card\">\n"
            "    <h2>Eyerus ONVIF Camera Stream</h2>\n"
            "    <div class=\"badge\">LIVE HLS STREAM</div>\n"
            "    <br>\n"
            "    <video id=\"video\" controls autoplay muted></video>\n"
            "    <p style=\"margin-top: 15px; color: #aaa;\">HLS Playlist: <a href=\"/live.m3u8\" target=\"_blank\">/live.m3u8</a></p>\n"
            "  </div>\n"
            "  <script>\n"
            "    var video = document.getElementById('video');\n"
            "    var videoSrc = '/live.m3u8';\n"
            "    function initPlayer() {\n"
            "      if (Hls.isSupported()) {\n"
            "        var hls = new Hls({ liveSyncDurationCount: 3 });\n"
            "        hls.loadSource(videoSrc);\n"
            "        hls.attachMedia(video);\n"
            "        hls.on(Hls.Events.MANIFEST_PARSED, function() { video.play(); });\n"
            "        hls.on(Hls.Events.ERROR, function(event, data) {\n"
            "          if (data.fatal) {\n"
            "            setTimeout(function() { hls.loadSource(videoSrc); }, 2000);\n"
            "          }\n"
            "        });\n"
            "      } else if (video.canPlayType('application/vnd.apple.mpegurl')) {\n"
            "        video.src = videoSrc;\n"
            "        video.addEventListener('loadedmetadata', function() { video.play(); });\n"
            "      }\n"
            "    }\n"
            "    setTimeout(initPlayer, 1500);\n"
            "  </script>\n"
            "</body>\n"
            "</html>\n";

        void handleClient(SOCKET clientSock, const std::string &hlsDir)
        {
            std::vector<char> reqBuf(4096);
            int bytesRecv = recv(clientSock, reqBuf.data(), static_cast<int>(reqBuf.size()) - 1, 0);
            if (bytesRecv <= 0)
            {
                closesocket(clientSock);
                return;
            }
            reqBuf[bytesRecv] = '\0';

            std::string request(reqBuf.data());
            std::string path = "/";

            size_t getPos = request.find("GET ");
            if (getPos != std::string::npos)
            {
                size_t endPos = request.find(' ', getPos + 4);
                if (endPos != std::string::npos)
                {
                    path = request.substr(getPos + 4, endPos - (getPos + 4));
                }
            }

            if (path == "/" || path == "/index.html")
            {
                std::string body = INDEX_HTML;
                std::ostringstream response;
                response << "HTTP/1.1 200 OK\r\n"
                         << "Content-Type: text/html; charset=utf-8\r\n"
                         << "Access-Control-Allow-Origin: *\r\n"
                         << "Content-Length: " << body.length() << "\r\n"
                         << "Connection: close\r\n\r\n"
                         << body;
                std::string resStr = response.str();
                send(clientSock, resStr.c_str(), static_cast<int>(resStr.length()), 0);
            }
            else
            {
                // Lay ten file (bo dau / o dau)
                std::string filename = (path.front() == '/') ? path.substr(1) : path;
                std::string filePath = hlsDir + "/" + filename;

                std::ifstream file(filePath, std::ios::binary);
                if (file.is_open())
                {
                    std::vector<char> fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    file.close();

                    std::string contentType = "application/octet-stream";
                    if (filename.find(".m3u8") != std::string::npos)
                    {
                        contentType = "application/vnd.apple.mpegurl";
                    }
                    else if (filename.find(".ts") != std::string::npos)
                    {
                        contentType = "video/mp2t";
                    }

                    std::ostringstream response;
                    response << "HTTP/1.1 200 OK\r\n"
                             << "Content-Type: " << contentType << "\r\n"
                             << "Access-Control-Allow-Origin: *\r\n"
                             << "Cache-Control: no-cache\r\n"
                             << "Content-Length: " << fileContent.size() << "\r\n"
                             << "Connection: close\r\n\r\n";

                    std::string header = response.str();
                    send(clientSock, header.c_str(), static_cast<int>(header.length()), 0);
                    send(clientSock, fileContent.data(), static_cast<int>(fileContent.size()), 0);
                }
                else
                {
                    std::string notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(clientSock, notFound.c_str(), static_cast<int>(notFound.length()), 0);
                }
            }

            closesocket(clientSock);
        }

        void serverWorker(int port, std::string hlsDir)
        {
            g_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (g_listenSocket == INVALID_SOCKET)
            {
                g_serverRunning = false;
                return;
            }

            int reuse = 1;
            setsockopt(g_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

            sockaddr_in serverAddr = {};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(static_cast<u_short>(port));
            serverAddr.sin_addr.s_addr = INADDR_ANY;

            if (bind(g_listenSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
            {
                closesocket(g_listenSocket);
                g_listenSocket = INVALID_SOCKET;
                g_serverRunning = false;
                return;
            }

            if (listen(g_listenSocket, SOMAXCONN) == SOCKET_ERROR)
            {
                closesocket(g_listenSocket);
                g_listenSocket = INVALID_SOCKET;
                g_serverRunning = false;
                return;
            }

            while (g_serverRunning)
            {
                fd_set readFds;
                FD_ZERO(&readFds);
                FD_SET(g_listenSocket, &readFds);

                timeval tv = {0, 200000}; // 200ms
                int sel = select(0, &readFds, nullptr, nullptr, &tv);

                if (sel > 0 && FD_ISSET(g_listenSocket, &readFds))
                {
                    sockaddr_in clientAddr = {};
                    int clientAddrLen = sizeof(clientAddr);
                    SOCKET clientSock = accept(g_listenSocket, (sockaddr *)&clientAddr, &clientAddrLen);
                    if (clientSock != INVALID_SOCKET)
                    {
                        std::thread(handleClient, clientSock, hlsDir).detach();
                    }
                }
            }

            if (g_listenSocket != INVALID_SOCKET)
            {
                closesocket(g_listenSocket);
                g_listenSocket = INVALID_SOCKET;
            }
        }

    } // anonymous namespace

    bool HlsServer::start(int port, const std::string &hlsDir)
    {
        if (g_serverRunning)
        {
            return true;
        }

        g_serverRunning = true;
        g_serverThread = std::thread(serverWorker, port, hlsDir);
        return true;
    }

    void HlsServer::stop()
    {
        if (g_serverRunning)
        {
            g_serverRunning = false;
            if (g_serverThread.joinable())
            {
                g_serverThread.join();
            }
        }
    }

    bool HlsServer::isRunning()
    {
        return g_serverRunning.load();
    }

    std::string HlsServer::getStreamUrl(int port, const std::string &playlistName)
    {
        return "http://localhost:" + std::to_string(port) + "/" + playlistName;
    }

    std::string HlsServer::getWebPlayerUrl(int port)
    {
        return "http://localhost:" + std::to_string(port) + "/index.html";
    }

} // namespace Eyerus
