#ifndef DISCOVERY_CAMERA_DEVICE_H
#define DISCOVERY_CAMERA_DEVICE_H

#include <string>
#include <vector>

namespace Eyerus
{

    class CameraDevice
    {
    public:
        std::string endpointReference;   // e.g. "urn:uuid:84ede3de-..."
        std::string ip;                  // IP camera (vd: "192.168.1.64")
        int port = 80;                   // Port ONVIF (vd: 80, 8000, 2020)
        std::vector<std::string> xaddrs; // Danh sách URL endpoint dịch vụ ONVIF
        std::vector<std::string> types;  // Kiểu thiết bị (NetworkVideoTransmitter, Device)
        std::vector<std::string> scopes; // Danh sách Scope URL

        // Dữ liệu bóc tách từ Scope
        std::string name;     // Tên Camera
        std::string hardware; // Model phần cứng
        std::string location; // Vị trí lắp đặt

        CameraDevice() = default;

        // Phân tích chuỗi scopes để trích xuất name, hardware, location
        void parseScopes();

        // Lấy URL endpoint chính
        std::string getMainXAddr() const;

        // Khóa định danh duy nhất (dùng để lọc trùng lặp khi scan)
        std::string getUniqueKey() const;

        // In thông tin tóm tắt ra console
        void printDetails() const;

    private:
        static std::string urlDecode(const std::string &str);
    };

} // namespace Eyerus

#endif // DISCOVERY_CAMERA_DEVICE_H
