# Feature: Camera Test & HLS Streamer (GStreamer Plugin)

Module này sử dụng **GStreamer** để nhận luồng từ Camera thật (RTSP) hoặc sinh luồng video thử nghiệm (**Test Camera** `videotestsrc`), sau đó đóng gói chuyển mã (transmux) thành luồng **HLS (HTTP Live Streaming)** cục bộ, kết hợp cùng HTTP Server nhúng để xem trực tiếp trên trình duyệt Web (Chrome, Edge) hoặc VLC Player.

---

## 1. Cấu trúc thư mục

```text
camera_test/
├── include/
│   ├── hls_streamer.h    # Điều khiển tiến trình GStreamer Pipeline (Test Camera & RTSP to HLS)
│   └── hls_server.h      # HTTP Server nhúng phục vụ file HLS (.m3u8, .ts) và Web Player HTML5
├── source/
│   ├── hls_streamer.cpp
│   └── hls_server.cpp
└── README.md             # Tài liệu hướng dẫn sử dụng này
```

---

## 2. Yêu cầu & Cài đặt GStreamer

1. Cài đặt **GStreamer 1.0 (MSVC hoặc MinGW 64-bit)** từ trang chủ: [gstreamer.freedesktop.org](https://gstreamer.freedesktop.org/download/).
2. Đảm bảo đã cài đủ gói **Complete** hoặc các plugin:
   - `videotestsrc`: Sinh hình ảnh kiểm thử (bóng nảy, đồng hồ thời gian).
   - `x264enc` & `h264parse`: Mã hóa video H.264 thời gian thực.
   - `hlssink`: Cắt phân đoạn video thành file `.ts` và tạo playlist `.m3u8`.
   - `rtspsrc` & `rtph264depay`: Thu luồng RTSP từ Camera ONVIF.
3. Thêm thư mục `bin` của GStreamer vào biến môi trường `PATH` của Windows (ví dụ: `C:\gstreamer\1.0\msvc_x86_64\bin`).
   - *Lưu ý:* Chương trình cũng tự động dò tìm trong các đường dẫn mặc định như `C:\gstreamer\1.0\msvc_x86_64\bin\gst-launch-1.0.exe`.

---

## 3. Kiến trúc luồng GStreamer HLS

```mermaid
flowchart LR
    subgraph Inputs ["Nguồn Video (Inputs)"]
        TestCam["Test Camera (videotestsrc)"]
        RtspCam["ONVIF RTSP Camera (rtspsrc)"]
    end

    subgraph Pipeline ["GStreamer Pipeline"]
        H264["x264enc / rtph264depay + h264parse"]
        HlsSink["hlssink (Segmenter)"]
    end

    subgraph Storage ["Thư mục HLS cục bộ"]
        M3U8["live.m3u8 (Playlist)"]
        TS["segment_00000.ts..."]
    end

    subgraph Server ["HlsServer (Embedded HTTP Server)"]
        HTTP["HTTP Server (Port 8080)"]
    end

    subgraph Clients ["Người xem (Viewers)"]
        Browser["Trình duyệt Web (hls.js)"]
        VLC["VLC / FFplay Player"]
    end

    TestCam --> H264
    RtspCam --> H264
    H264 --> HlsSink
    HlsSink --> M3U8
    HlsSink --> TS
    M3U8 --> HTTP
    TS --> HTTP
    HTTP --> Browser
    HTTP --> VLC
```

---

## 4. Hướng dẫn sử dụng API trong C++

### Ví dụ 1: Chạy Test Camera đẩy lên HLS & Mở Web Server xem trên trình duyệt

```cpp
#include "hls_streamer.h"
#include "hls_server.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    Eyerus::HlsStreamer streamer;
    Eyerus::StreamConfig cfg;
    cfg.outputDir = "hls";
    cfg.playlistName = "live.m3u8";
    cfg.width = 1280;
    cfg.height = 720;
    cfg.fps = 25;

    // 1. Khởi chạy luồng Test Camera
    std::cout << "[*] Khoi chay Test Camera HLS...\n";
    if (!streamer.startTestStream(cfg)) {
        std::cerr << "[!] Loi: Khong the khoi chay GStreamer!\n";
        return 1;
    }

    // 2. Khởi chạy Local HTTP Server tại port 8080
    Eyerus::HlsServer::start(8080, "hls");

    std::cout << "[+] Da phat luong HLS thanh cong!\n";
    std::cout << "    - Web Player: " << Eyerus::HlsServer::getWebPlayerUrl(8080) << "\n";
    std::cout << "    - HLS URL   : " << Eyerus::HlsServer::getStreamUrl(8080) << "\n";
    std::cout << "[*] Mo trinh duyet Web hoac VLC de xem video. Nhan Enter de dung...\n";

    std::cin.get();

    // 3. Dung luong
    streamer.stopStream();
    Eyerus::HlsServer::stop();
    return 0;
}
```

---

### Ví dụ 2: Lấy luồng RTSP từ Camera ONVIF thật đẩy lên HLS

Khi camera thật đã có URL RTSP (ví dụ: `rtsp://admin:Admin12345@192.168.1.64:554/stream1`), bạn chỉ cần gọi `startRtspStream`:

```cpp
std::string rtspUrl = "rtsp://admin:Admin12345@192.168.1.64:554/stream1";
streamer.startRtspStream(rtspUrl, cfg);
```
*Lưu ý: Luồng RTSP H.264 từ camera sẽ được `hlssink` cắt thành phân đoạn trực tiếp mà **không cần mã hóa lại**, giúp tiết kiệm tối đa CPU.*

---

## 5. Cách xem video HLS

1. **Xem trên Trình duyệt Web (Chrome / Edge / Firefox / Safari):**
   - Mở địa chỉ: [http://localhost:8080](http://localhost:8080)
   - Trang web tích hợp sẵn thư viện `hls.js`, tự động phát video độ nét cao và độ trễ thấp.
2. **Xem bằng VLC Media Player:**
   - Mở VLC $\rightarrow$ `Media` $\rightarrow$ `Open Network Stream...`
   - Dán URL: `http://localhost:8080/live.m3u8` $\rightarrow$ Nhấn `Play`.
3. **Xem bằng FFplay:**
   ```bash
   ffplay http://localhost:8080/live.m3u8
   ```

