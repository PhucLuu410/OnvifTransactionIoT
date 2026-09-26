# Feature: Device (ONVIF Device Service & Ping)

Module này chịu trách nhiệm gửi các yêu cầu SOAP chuẩn hóa đến Endpoint của camera ONVIF để thực hiện **Ping**, kiểm tra tính hợp lệ của tài khoản/mật khẩu và truy vấn thông số thiết bị.

---

## 1. Cấu trúc thư mục

```text
device/
├── include/
│   └── onvif_device.h   # Định nghĩa cấu trúc PingResult và API OnvifDevice
├── source/
│   └── onvif_device.cpp # Thực thi gọi SOAP GetDeviceInformation & GetSystemDateAndTime
└── README.md
```

---

## 2. Ý nghĩa của chức năng "Ping Camera"

Trong chuẩn ONVIF, "Ping" không phải là gói tin ICMP ping thông thường của hệ điều hành, mà là một lượt trao đổi nghiệp vụ mức ứng dụng (Application-level Ping) nhằm kiểm chứng:
1. **Khả năng kết nối**: Cổng HTTP của camera (thường là 80, 8000, 2020) có mở và phản hồi hay không.
2. **Độ trễ (Latency)**: Đo thời gian phản hồi (round-trip time) từ lúc gửi request đến khi nhận response từ camera (đơn vị: mili-giây).
3. **Tính hợp lệ của Tài khoản & Mật khẩu**: Gắn bản tin xác thực `WS-Security UsernameToken` vào request `GetDeviceInformation`:
   - Nếu tài khoản đúng: Camera trả về mã HTTP `200 OK` kèm thông tin phần cứng (Manufacturer, Model, FirmwareVersion, SerialNumber).
   - Nếu tài khoản sai: Camera trả về mã HTTP `401 Unauthorized` hoặc SOAP Fault `NotAuthorized`.
   - Nếu camera không phản hồi: Báo lỗi Timeout hoặc Connection Refused.

---

## 3. Hướng dẫn sử dụng API

### `OnvifDevice::ping(xaddr, username, password, timeoutMs)`

```cpp
#include "onvif_device.h"
#include <iostream>

int main() {
    std::string xaddr = "http://192.168.1.64:80/onvif/device_service";
    
    // Ping camera kèm kiểm tra tài khoản và mật khẩu
    Eyerus::PingResult result = Eyerus::OnvifDevice::ping(xaddr, "admin", "Admin12345", 4000);

    // In kết quả tóm tắt
    result.printSummary();

    if (result.isAuthenticated) {
        std::cout << "Hang san xuat: " << result.manufacturer << "\n";
        std::cout << "Model: " << result.model << "\n";
        std::cout << "Firmware: " << result.firmwareVersion << "\n";
        std::cout << "Serial: " << result.serialNumber << "\n";
        std::cout << "Do tre Ping: " << result.latencyMs << " ms\n";
    }
    return 0;
}
```

