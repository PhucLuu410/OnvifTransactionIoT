# Feature: Network

Module này cung cấp các tiện ích mạng mức thấp (Low-level Networking) dành riêng cho chuẩn ONVIF trên nền tảng Windows Winsock2.

---

## 1. Cấu trúc thư mục

```text
network/
├── include/
│   ├── net_utils.h      # Dò tìm card mạng (NICs), phân tích URL endpoint
│   ├── udp_socket.h     # Socket UDP multicast/unicast non-blocking kèm timeout
│   └── http_client.h    # HTTP/1.1 Client gửi SOAP payload kèm đo độ trễ (latency)
├── source/
│   ├── net_utils.cpp
│   ├── udp_socket.cpp
│   └── http_client.cpp
└── README.md
```

---

## 2. Hướng dẫn sử dụng API

### 2.1. NetUtils (`net_utils.h`)
- **`NetUtils::getActiveIPv4Interfaces()`**:
  - Dùng Windows API `GetAdaptersAddresses` để lấy danh sách các card mạng (Wi-Fi, Ethernet, VPN) đang kết nối, bỏ qua card ảo và loopback.
  - *Ví dụ:*
    ```cpp
    auto nics = Eyerus::NetUtils::getActiveIPv4Interfaces();
    for (const auto& nic : nics) {
        std::cout << nic.ipAddress << " - " << nic.description << std::endl;
    }
    ```
- **`NetUtils::parseUrl(url, outHost, outPort, outPath)`**:
  - Tách URL ONVIF endpoint (ví dụ: `http://192.168.1.64:8000/onvif/device_service`) thành IP host, port và path.

### 2.2. UdpSocket (`udp_socket.h`)
- Hỗ trợ gửi multicast tới `239.255.255.250:3702` và nhận unicast phản hồi từ camera với timeout không gây treo chương trình:
  ```cpp
  Eyerus::UdpSocket sock;
  sock.bindTo("192.168.1.100", 0); // Gắn với card mạng chỉ định, port ngẫu nhiên
  sock.sendTo(probeData, "239.255.255.250", 3702);

  std::string response, senderIp;
  int senderPort;
  if (sock.receiveFrom(response, senderIp, senderPort, 2000)) {
      // Nhận phản hồi thành công trong 2000ms
  }
  ```

### 2.3. HttpClient (`http_client.h`)
- Gửi bản tin SOAP qua chuẩn HTTP/1.1 POST kèm đo thời gian phản hồi:
  ```cpp
  Eyerus::HttpResponse resp;
  bool ok = Eyerus::HttpClient::postSoap(xaddr, soapBody, resp, 4000);
  if (ok && resp.statusCode == 200) {
      std::cout << "Ping latency: " << resp.roundTripMs << " ms\n";
      std::cout << "XML Body: " << resp.body << "\n";
  }
  ```

