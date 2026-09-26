# Feature: Discovery (WS-Discovery Scanner)

Module này thực hiện giao thức **WS-Discovery** (Web Services Dynamic Discovery) để tự động phát hiện mọi camera và thiết bị ONVIF trong mạng LAN.

---

## 1. Cấu trúc thư mục

```text
discovery/
├── include/
│   ├── camera_device.h   # Data Model cho camera (IP, Port, XAddrs, Scopes, Name, Model)
│   └── ws_discovery.h    # Giao diện dò quét mạng WS-Discovery
├── source/
│   ├── camera_device.cpp
│   └── ws_discovery.cpp
└── README.md
```

---

## 2. Cơ chế hoạt động

1. Ứng dụng tự động lấy danh sách IP của tất cả các card mạng (Wi-Fi, Ethernet).
2. Tạo bản tin SOAP Probe với `w:Action` = `http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe` và UUID v4 ngẫu nhiên.
3. Gửi multicast UDP đến địa chỉ **`239.255.255.250:3702`**.
4. Lắng nghe phản hồi unicast `ProbeMatch` từ các camera.
5. Parse trực tiếp dữ liệu XML trong bộ nhớ RAM bằng `pugixml` (xử lý độc lập tiền tố namespace, hỗ trợ camera của mọi hãng như Dahua, Hikvision, Axis, Ezviz, Imou,...).
6. Tự động loại bỏ trùng lặp và trích xuất: Tên camera, Model phần cứng, Vị trí lắp đặt, Endpoint URL (`XAddrs`).

---

## 3. Hướng dẫn sử dụng API

### `WsDiscovery::scanNetwork(...)`
Quét camera trên toàn bộ dải mạng với callback tiến trình và camera tìm thấy:

```cpp
#include "ws_discovery.h"
#include <iostream>

int main() {
    auto cameras = Eyerus::WsDiscovery::scanNetwork(
        3,                  // Quét trong 3 giây
        "",                 // "" = Quét mọi card mạng; hoặc điền IP cụ thể vd "192.168.1.100"
        [](const Eyerus::CameraDevice& cam) {
            std::cout << "[+] Phat hien: " << cam.name << " tai " << cam.ip << ":" << cam.port << "\n";
        },
        [](const std::string& status) {
            std::cout << "[INFO] " << status << "\n";
        }
    );

    std::cout << "Tong so camera tim thay: " << cameras.size() << "\n";
    for (const auto& cam : cameras) {
        cam.printDetails();
    }
    return 0;
}
```

