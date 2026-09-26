#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include "ws_discovery.h"
#include "onvif_device.h"
#include "net_utils.h"
#include "hls_streamer.h"
#include "hls_server.h"

void printBanner()
{
    std::cout << "==================================================================\n";
    std::cout << "          EYERUS - ONVIF CAMERA SCANNER, PING & HLS STREAMER      \n";
    std::cout << "==================================================================\n\n";
}

void printHelp(const char *progName)
{
    std::cout << "Cach su dung:\n";
    std::cout << "  1. Chay truc tiep menu tuong tac:\n";
    std::cout << "     " << progName << "\n\n";
    std::cout << "  2. Quet camera theo thoi gian chi dinh:\n";
    std::cout << "     " << progName << " --scan [so_giay_timeout]\n";
    std::cout << "     Vi du: " << progName << " --scan 5\n\n";
    std::cout << "  3. Ping truc tiep camera kem tai khoan/mat khau:\n";
    std::cout << "     " << progName << " --ping <ip_hoac_url> <username> <password> [port]\n";
    std::cout << "     Vi du: " << progName << " --ping 192.168.1.64 admin Admin12345 80\n\n";
    std::cout << "  4. Chay Test Camera GStreamer day len HLS Local:\n";
    std::cout << "     " << progName << " --test-hls [port_http]\n";
    std::cout << "     Vi du: " << progName << " --test-hls 8080\n\n";
    std::cout << "  5. Chuyen luong Camera RTSP sang HLS Local:\n";
    std::cout << "     " << progName << " --stream-rtsp <rtsp_url> [port_http]\n";
    std::cout << "     Vi du: " << progName << " --stream-rtsp rtsp://admin:pass@192.168.1.64:554/live 8080\n\n";
}

void executePing(const std::string &xaddr, const std::string &username, const std::string &password)
{
    std::cout << "\n[*] Dang gui yeu cau Ping xac thuc den: " << xaddr << " ...\n";
    Eyerus::PingResult res = Eyerus::OnvifDevice::ping(xaddr, username, password, 4000);
    res.printSummary();
}

void runTestHlsStreaming(int port = 8080)
{
    Eyerus::HlsStreamer streamer;
    Eyerus::StreamConfig cfg;
    cfg.outputDir = "hls";
    cfg.playlistName = "live.m3u8";

    std::cout << "[*] Dang khoi chay Test Camera GStreamer...\n";
    if (!streamer.startTestStream(cfg))
    {
        std::cout << "[!] Loi: Khong the khoi chay GStreamer! Hay chac chan rang ban da cai dat GStreamer tren may.\n";
        std::cout << "    Ban co the them duong dan thu muc bin cua GStreamer vao PATH hoac dat bien moi truong GST_PATH.\n";
        return;
    }

    Eyerus::HlsServer::start(port, "hls");

    std::cout << "\n==================================================================\n";
    std::cout << "                   LUONG HLS LOCAL DANG PHAT                      \n";
    std::cout << "==================================================================\n";
    std::cout << "  [>] Web Player (Xem tren trinh duyet) : " << Eyerus::HlsServer::getWebPlayerUrl(port) << "\n";
    std::cout << "  [>] URL luong HLS (Xem tren VLC)      : " << Eyerus::HlsServer::getStreamUrl(port) << "\n";
    std::cout << "  [>] Thư muc chua file .m3u8 & .ts     : ./hls/\n";
    std::cout << "==================================================================\n";
    std::cout << "[*] Nhan ENTER de dung phat luong...\n";

    std::string line;
    std::getline(std::cin, line);

    streamer.stopStream();
    Eyerus::HlsServer::stop();
    std::cout << "[+] Da dung phat luong HLS.\n";
}

void runRtspHlsStreaming(const std::string &rtspUrl, int port = 8080)
{
    Eyerus::HlsStreamer streamer;
    Eyerus::StreamConfig cfg;
    cfg.outputDir = "hls";
    cfg.playlistName = "live.m3u8";

    std::cout << "[*] Dang khoi chay chuyen luong RTSP sang HLS...\n";
    std::cout << "    RTSP Source: " << rtspUrl << "\n";
    if (!streamer.startRtspStream(rtspUrl, cfg))
    {
        std::cout << "[!] Loi: Khong the khoi chay GStreamer cho luong RTSP.\n";
        return;
    }

    Eyerus::HlsServer::start(port, "hls");

    std::cout << "\n==================================================================\n";
    std::cout << "                   LUONG HLS LOCAL DANG PHAT                      \n";
    std::cout << "==================================================================\n";
    std::cout << "  [>] Web Player (Xem tren trinh duyet) : " << Eyerus::HlsServer::getWebPlayerUrl(port) << "\n";
    std::cout << "  [>] URL luong HLS (Xem tren VLC)      : " << Eyerus::HlsServer::getStreamUrl(port) << "\n";
    std::cout << "==================================================================\n";
    std::cout << "[*] Nhan ENTER de dung phat luong...\n";

    std::string line;
    std::getline(std::cin, line);

    streamer.stopStream();
    Eyerus::HlsServer::stop();
    std::cout << "[+] Da dung phat luong HLS.\n";
}

void scanAndPingFlow()
{
    // Hien thi danh sach card mang
    std::cout << "[*] Danh sach card mang tren may:\n";
    auto nics = Eyerus::NetUtils::getActiveIPv4Interfaces();
    for (size_t i = 0; i < nics.size(); ++i)
    {
        std::cout << "    [" << (i + 1) << "] " << nics[i].ipAddress << " - " << nics[i].description << "\n";
    }
    std::cout << "\n";

    // Quet camera
    std::cout << "[*] Dang tu dong quet camera ONVIF trong mang (3 giay)...\n";
    auto devices = Eyerus::WsDiscovery::scanNetwork(
        3,
        "",
        [](const Eyerus::CameraDevice &dev)
        {
            std::cout << "    [+] Phat hien: " << dev.name
                      << " @ " << dev.ip << ":" << dev.port
                      << " (" << dev.hardware << ")\n";
        },
        [](const std::string &msg)
        {
            std::cout << "    [INFO] " << msg << "\n";
        });

    // In bang ket qua quet
    std::cout << "\n==================================================================\n";
    std::cout << "                      DANH SACH CAMERA ONVIF                      \n";
    std::cout << "==================================================================\n";

    if (devices.empty())
    {
        std::cout << "[!] Khong tim thay camera nao phan hoi tu dong.\n";
        std::cout << "    Ban co the nhap dia chi IP thu cong de Ping kiem tra.\n\n";

        std::cout << "[?] Ban co muon nhap IP thu cong de Ping khong? (y/n): ";
        std::string choice;
        std::getline(std::cin, choice);
        if (choice == "y" || choice == "Y")
        {
            std::string ip;
            std::cout << "Nhap IP Camera: ";
            std::getline(std::cin, ip);

            std::string portStr;
            std::cout << "Nhap Port (mac dinh 80): ";
            std::getline(std::cin, portStr);
            int port = 80;
            if (!portStr.empty())
            {
                try
                {
                    port = std::stoi(portStr);
                }
                catch (...)
                {
                    port = 80;
                }
            }

            std::string user;
            std::cout << "Nhap Username (mac dinh admin): ";
            std::getline(std::cin, user);
            if (user.empty())
                user = "admin";

            std::string pass;
            std::cout << "Nhap Password: ";
            std::getline(std::cin, pass);

            std::string xaddr = "http://" + ip + ":" + std::to_string(port) + "/onvif/device_service";
            executePing(xaddr, user, pass);
        }
    }
    else
    {
        std::cout << std::left
                  << std::setw(5) << "STT"
                  << std::setw(24) << "Ten Camera"
                  << std::setw(22) << "IP:Port"
                  << std::setw(16) << "Phan cung"
                  << "\n";
        std::cout << "------------------------------------------------------------------\n";

        for (size_t i = 0; i < devices.size(); ++i)
        {
            const auto &dev = devices[i];
            std::string ipport = dev.ip + ":" + std::to_string(dev.port);
            std::string name = (dev.name.length() > 22) ? dev.name.substr(0, 19) + "..." : dev.name;
            std::string hw = (dev.hardware.length() > 14) ? dev.hardware.substr(0, 11) + "..." : dev.hardware;

            std::cout << std::left
                      << std::setw(5) << (i + 1)
                      << std::setw(24) << name
                      << std::setw(22) << ipport
                      << std::setw(16) << hw
                      << "\n";
        }
        std::cout << "------------------------------------------------------------------\n\n";

        while (true)
        {
            std::cout << "[?] Nhap so STT camera muon Ping (1-" << devices.size() << "), 'a' de Ping tat ca, hoac 'q' de quay lai: ";
            std::string input;
            if (!std::getline(std::cin, input) || input == "q" || input == "Q")
            {
                break;
            }

            std::string username;
            std::cout << "[?] Nhap Username (mac dinh: admin): ";
            std::getline(std::cin, username);
            if (username.empty())
                username = "admin";

            std::string password;
            std::cout << "[?] Nhap Password: ";
            std::getline(std::cin, password);

            if (input == "a" || input == "A")
            {
                for (size_t i = 0; i < devices.size(); ++i)
                {
                    std::cout << "\n>>> PING THIET BI #" << (i + 1) << " (" << devices[i].name << " - " << devices[i].ip << "):\n";
                    executePing(devices[i].getMainXAddr(), username, password);
                }
            }
            else
            {
                try
                {
                    int idx = std::stoi(input) - 1;
                    if (idx >= 0 && idx < static_cast<int>(devices.size()))
                    {
                        executePing(devices[idx].getMainXAddr(), username, password);
                    }
                    else
                    {
                        std::cout << "[!] STT khong hop le.\n";
                    }
                }
                catch (...)
                {
                    std::cout << "[!] Lua chon khong hop le.\n";
                }
            }

            std::cout << "\n";
        }
    }
}

int main(int argc, char *argv[])
{
    // 1. Kiem tra tham so dong lenh
    if (argc >= 2)
    {
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h")
        {
            printBanner();
            printHelp(argv[0]);
            return 0;
        }

        if (arg1 == "--scan")
        {
            printBanner();
            int timeoutSec = 3;
            if (argc >= 3)
            {
                try
                {
                    timeoutSec = std::stoi(argv[2]);
                }
                catch (...)
                {
                    timeoutSec = 3;
                }
            }
            std::cout << "[*] Bat dau quet ONVIF trong " << timeoutSec << " giay...\n";
            auto devices = Eyerus::WsDiscovery::scanNetwork(timeoutSec);
            std::cout << "\n[+] Tim thay " << devices.size() << " thiet bi.\n";
            for (size_t i = 0; i < devices.size(); ++i)
            {
                std::cout << "\n[Thiet bi #" << (i + 1) << "]\n";
                devices[i].printDetails();
            }
            return 0;
        }

        if (arg1 == "--ping")
        {
            if (argc < 5)
            {
                std::cout << "[!] Thieu tham so. Cu phap: " << argv[0] << " --ping <ip_hoac_url> <username> <password> [port]\n";
                return 1;
            }
            std::string target = argv[2];
            std::string username = argv[3];
            std::string password = argv[4];
            int port = 80;
            if (argc >= 6)
            {
                try
                {
                    port = std::stoi(argv[5]);
                }
                catch (...)
                {
                    port = 80;
                }
            }

            std::string xaddr = target;
            if (target.find("://") == std::string::npos)
            {
                xaddr = "http://" + target + ":" + std::to_string(port) + "/onvif/device_service";
            }

            printBanner();
            executePing(xaddr, username, password);
            return 0;
        }

        if (arg1 == "--test-hls")
        {
            int port = 8080;
            if (argc >= 3)
            {
                try
                {
                    port = std::stoi(argv[2]);
                }
                catch (...)
                {
                    port = 8080;
                }
            }
            printBanner();
            runTestHlsStreaming(port);
            return 0;
        }

        if (arg1 == "--stream-rtsp")
        {
            if (argc < 3)
            {
                std::cout << "[!] Thieu tham so RTSP URL. Cu phap: " << argv[0] << " --stream-rtsp <rtsp_url> [port_http]\n";
                return 1;
            }
            std::string rtspUrl = argv[2];
            int port = 8080;
            if (argc >= 4)
            {
                try
                {
                    port = std::stoi(argv[3]);
                }
                catch (...)
                {
                    port = 8080;
                }
            }
            printBanner();
            runRtspHlsStreaming(rtspUrl, port);
            return 0;
        }
    }

    // 2. Interactive Menu
    while (true)
    {
        printBanner();
        std::cout << "MENU CHUC NANG:\n";
        std::cout << "  [1] Tu dong quet & Ping kiem tra Camera ONVIF trong mang LAN\n";
        std::cout << "  [2] Chay Test Camera GStreamer day len HLS Local (Co Web Player)\n";
        std::cout << "  [3] Chuyen luong Camera RTSP sang HLS Local\n";
        std::cout << "  [q] Thoat chuong trinh\n\n";
        std::cout << "[?] Chon chuc nang (1-3, q): ";

        std::string choice;
        if (!std::getline(std::cin, choice) || choice == "q" || choice == "Q")
        {
            break;
        }

        if (choice == "1")
        {
            std::cout << "\n";
            scanAndPingFlow();
        }
        else if (choice == "2")
        {
            std::cout << "\n[?] Nhap Port cho Web Server HLS (mac dinh 8080): ";
            std::string pStr;
            std::getline(std::cin, pStr);
            int p = 8080;
            if (!pStr.empty())
            {
                try
                {
                    p = std::stoi(pStr);
                }
                catch (...)
                {
                    p = 8080;
                }
            }
            runTestHlsStreaming(p);
        }
        else if (choice == "3")
        {
            std::cout << "\n[?] Nhap URL RTSP cua Camera (vd: rtsp://admin:pass@192.168.1.64:554/stream): ";
            std::string rtsp;
            std::getline(std::cin, rtsp);
            if (!rtsp.empty())
            {
                std::cout << "[?] Nhap Port cho Web Server HLS (mac dinh 8080): ";
                std::string pStr;
                std::getline(std::cin, pStr);
                int p = 8080;
                if (!pStr.empty())
                {
                    try
                    {
                        p = std::stoi(pStr);
                    }
                    catch (...)
                    {
                        p = 8080;
                    }
                }
                runRtspHlsStreaming(rtsp, p);
            }
        }

        std::cout << "\nNhan ENTER de tiep tuc...";
        std::string dummy;
        std::getline(std::cin, dummy);
    }

    std::cout << "\n[*] Tam biet!\n";
    return 0;
}