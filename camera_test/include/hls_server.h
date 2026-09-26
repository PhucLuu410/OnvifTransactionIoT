#ifndef CAMERA_TEST_HLS_SERVER_H
#define CAMERA_TEST_HLS_SERVER_H

#include <string>

namespace Eyerus
{

    class HlsServer
    {
    public:
        // Khởi chạy HTTP Server cục bộ phát video HLS và giao diện xem trực tiếp Web Player
        static bool start(int port = 8080, const std::string &hlsDir = "hls");

        // Dừng HTTP Server
        static void stop();

        // Kiểm tra Server có đang chạy không
        static bool isRunning();

        // Lấy URL phát luồng HLS (vd: http://localhost:8080/live.m3u8)
        static std::string getStreamUrl(int port = 8080, const std::string &playlistName = "live.m3u8");

        // Lấy URL Web Player (vd: http://localhost:8080/index.html)
        static std::string getWebPlayerUrl(int port = 8080);
    };

} // namespace Eyerus

#endif // CAMERA_TEST_HLS_SERVER_H
