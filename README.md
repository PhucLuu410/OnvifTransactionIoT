# Eyerus - ONVIF Camera Scanner, Ping & HLS Streamer

Ứng dụng C++ chuyên nghiệp hỗ trợ:
1. **Dò quét tự động (WS-Discovery)** toàn bộ camera ONVIF trong mạng nội bộ.
2. **Ping xác thực (WS-Security UsernameToken)** bằng tài khoản & mật khẩu để kiểm tra độ trễ mạng và lấy thông tin phần cứng.
3. **Plugin Test Camera & HLS Streaming (GStreamer)**: Tự tạo luồng video thử nghiệm hoặc thu luồng RTSP từ Camera thật, đóng gói thành chuẩn **HLS (HTTP Live Streaming)** cục bộ và tích hợp sẵn Web Server để xem trực tiếp trên trình duyệt Web (Chrome, Edge) hoặc VLC Player.

---

## 1. Cấu trúc thư mục dự án theo tính năng (Feature-based Architecture)

Mỗi thư mục chức năng hoàn toàn độc lập và chứa:
- `include/`: Chứa file header giao diện API (`.h`).
- `source/`: Chứa mã nguồn thực thi chi tiết (`.cpp`).
- `README.md`: Hướng dẫn chi tiết cách dùng API của riêng tính năng đó.

```text
Eyerus/
│
├── discovery/                     # [TÍNH NĂNG 1] Dò quét camera ONVIF trong LAN
│   ├── include/
│   │   ├── camera_device.h        # Model lưu thông tin Camera (IP, Port, Scopes, XAddrs)
│   │   └── ws_discovery.h         # API gửi Probe Multicast & bóc tách ProbeMatch XML
│   ├── source/
│   │   ├── camera_device.cpp
│   │   └── ws_discovery.cpp
│   └── README.md                  # Hướng dẫn API Discovery
│
├── auth/                          # [TÍNH NĂNG 2] Xác thực ONVIF WS-Security
│   ├── include/
│   │   ├── sha1.h                 # Thuật toán băm SHA-1 (RFC 3174)
│   │   ├── base64.h               # Mã hóa Base64
│   │   └── ws_security.h          # Sinh XML WS-Security Header (PasswordDigest, Nonce, UTC)
│   ├── source/
│   │   ├── sha1.cpp
│   │   ├── base64.cpp
│   │   └── ws_security.cpp
│   └── README.md                  # Hướng dẫn API Auth & Chuẩn PasswordDigest
│
├── device/                        # [TÍNH NĂNG 3] Nghiệp vụ ONVIF Device & Ping Camera
│   ├── include/
│   │   └── onvif_device.h         # API Ping, GetDeviceInformation, GetSystemDateAndTime
│   ├── source/
│   │   └── onvif_device.cpp
│   └── README.md                  # Hướng dẫn API Device & Ping
│
├── camera_test/                   # [TÍNH NĂNG 4] Plugin GStreamer & HLS Local Streamer
│   ├── include/
│   │   ├── hls_streamer.h         # Điều khiển GStreamer Pipeline (Test Camera & RTSP to HLS)
│   │   └── hls_server.h           # Embedded HTTP Server phục vụ file HLS và Web Player HTML5
│   ├── source/
│   │   ├── hls_streamer.cpp
│   │   └── hls_server.cpp
│   └── README.md                  # Hướng dẫn API Camera Test & HLS Streaming
│
├── network/                       # [HẠ TẦNG] Socket và Tiện ích mạng
│   ├── include/
│   │   ├── net_utils.h            # Dò tìm card mạng máy (Wi-Fi, LAN) & parse URL
│   │   ├── udp_socket.h           # Socket UDP Multicast 239.255.255.250:3702
│   │   └── http_client.h          # Client HTTP/1.1 POST gửi SOAP request kèm đo độ trễ
│   ├── source/
│   │   ├── net_utils.cpp
│   │   ├── udp_socket.cpp
│   │   └── http_client.cpp
│   └── README.md                  # Hướng dẫn API Network
│
├── xml/                           # [THƯ VIỆN BÊN THỨ 3] Bộ phân tích XML pugixml
│   ├── include/
│   │   ├── pugiconfig.hpp
│   │   └── pugixml.hpp
│   └── source/
│       └── pugixml.cpp
│
├── main.cpp                       # Chương trình chính (Interactive Menu & CLI Flags)
├── makefile                       # Script biên dịch tự động MinGW g++
└── README.md                      # Tài liệu tổng thể này
```

---

## 2. Yêu cầu & Biên dịch

### Yêu cầu:
- Hệ điều hành: **Windows 10 / 11**
- Trình biên dịch: **MinGW-W64 (`g++`)** hỗ trợ C++11 trở lên.
- Đã cài đặt **GStreamer 1.0** trên máy (khuyên dùng bản Complete để có `videotestsrc`, `x264enc`, `hlssink`, `rtspsrc`).
  - Nếu `gst-launch-1.0` chưa nằm trong `PATH`, chương trình sẽ tự động dò tìm trong các thư mục mặc định như `C:\gstreamer\1.0\msvc_x86_64\bin\gst-launch-1.0.exe` hoặc bạn có thể đặt biến môi trường `GST_PATH`.

### Lệnh biên dịch:
```powershell
mingw32-make
```

Để dọn dẹp các file sinh ra:
```powershell
mingw32-make clean
```

---

## 3. Hướng dẫn chạy chương trình

### Cách 1: Chạy Menu tương tác (Khuyên dùng)
```powershell
.\main.exe
```

Khi chạy, ứng dụng sẽ hiển thị menu:
```text
MENU CHUC NANG:
  [1] Tu dong quet & Ping kiem tra Camera ONVIF trong mang LAN
  [2] Chay Test Camera GStreamer day len HLS Local (Co Web Player)
  [3] Chuyen luong Camera RTSP sang HLS Local
  [q] Thoat chuong trinh
```

---

### Cách 2: Chạy trực tiếp qua tham số dòng lệnh (CLI)

#### 1. Chạy Test Camera GStreamer đẩy lên HLS Local:
```powershell
.\main.exe --test-hls 8080
```
- Khởi tạo luồng video test (đồng hồ thời gian + bóng nảy) qua GStreamer.
- Tự động cắt luồng thành các file `.ts` và playlist `.m3u8` lưu tại thư mục `./hls/`.
- Mở server HTTP nhúng tại cổng `8080`.
- **Xem trực tiếp:**
  - Trình duyệt Web (Chrome / Edge): Mở `http://localhost:8080` (có sẵn Web Player HTML5 tích hợp `hls.js`).
  - VLC Media Player: Mở URL `http://localhost:8080/live.m3u8`.

#### 2. Chuyển luong RTSP từ Camera thật sang HLS Local:
```powershell
.\main.exe --stream-rtsp rtsp://admin:Admin12345@192.168.1.64:554/stream1 8080
```

#### 3. Quét camera ONVIF tự động trong $N$ giây:
```powershell
.\main.exe --scan 3
```

#### 4. Ping xác thực trực tiếp camera qua IP/URL:
```powershell
.\main.exe --ping 192.168.1.64 admin Admin12345 80
```

---

## 4. Danh sách tài liệu chi tiết từng module
- [discovery/README.md](file:///c:/Users/luuph/OneDrive/Desktop/Eyerus/discovery/README.md): Hướng dẫn API dò quét camera WS-Discovery.
- [auth/README.md](file:///c:/Users/luuph/OneDrive/Desktop/Eyerus/auth/README.md): Hướng dẫn API mã hóa bảo mật WS-Security PasswordDigest.
- [device/README.md](file:///c:/Users/luuph/OneDrive/Desktop/Eyerus/device/README.md): Hướng dẫn API Ping và truy vấn thông số camera.
- [camera_test/README.md](file:///c:/Users/luuph/OneDrive/Desktop/Eyerus/camera_test/README.md): Hướng dẫn API Test Camera, GStreamer Pipeline và Web Player HLS.
- [network/README.md](file:///c:/Users/luuph/OneDrive/Desktop/Eyerus/network/README.md): Hướng dẫn API Socket UDP, HTTP Client và tiện ích mạng.
