#ifndef DEVICE_ONVIF_DEVICE_H
#define DEVICE_ONVIF_DEVICE_H

#include <string>

namespace Eyerus
{

    struct PingResult
    {
        bool isReachable = false;     // Thiết bị có kết nối được không
        bool isAuthenticated = false; // Tài khoản & Mật khẩu có hợp lệ không
        int latencyMs = 0;            // Độ trễ phản hồi (ms)
        int httpStatusCode = 0;       // Mã phản hồi HTTP (200, 401, 500,...)

        // Thông tin chi tiết thiết bị (khi xác thực thành công)
        std::string manufacturer;    // Hãng sản xuất (Hikvision, Dahua, Axis,...)
        std::string model;           // Model thiết bị
        std::string firmwareVersion; // Phiên bản firmware
        std::string serialNumber;    // Số serial
        std::string hardwareId;      // Mã phần cứng

        std::string errorMessage; // Thông báo lỗi nếu thất bại

        void printSummary() const;
    };

    class OnvifDevice
    {
    public:
        // Ping kiểm tra camera kèm xác thực tài khoản/mật khẩu
        static PingResult ping(
            const std::string &xaddr,
            const std::string &username = "",
            const std::string &password = "",
            int timeoutMs = 4000);

        // Lấy thông tin chi tiết camera (GetDeviceInformation)
        static bool getDeviceInformation(
            const std::string &xaddr,
            const std::string &username,
            const std::string &password,
            PingResult &outResult,
            int timeoutMs = 4000);

        // Kiểm tra thời gian hệ thống camera (GetSystemDateAndTime, không cần mật khẩu)
        static bool getSystemDateAndTime(
            const std::string &xaddr,
            std::string &outDateString,
            int timeoutMs = 4000);
    };

} // namespace Eyerus

#endif // DEVICE_ONVIF_DEVICE_H
