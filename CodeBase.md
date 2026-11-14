# Eyerus: sổ tay code lại theo service

File này là kế hoạch để tự code lại Eyerus từng service một. Mỗi task nhỏ, có cách kiểm thử riêng: làm xong, chạy
kiểm thử đạt thì mới đánh dấu `[x]` và ghi vào [Nhật ký](#9-nhật-ký-tiến-độ).

- Code gốc để tham khảo: `/home/phucluu/Workspace/eyerus` (bản V2 2.2.24), gọi tắt là **V2** bên dưới.
- Code mới: `/home/phucluu/Workspace/Eyerus` (thư mục này).

## Mục lục

1. [Kiến trúc mới](#1-kiến-trúc-mới)
2. [Quy ước chung](#2-quy-ước-chung)
3. [Giao thức IPC giữa các service](#3-giao-thức-ipc-giữa-các-service)
4. [File trên máy đích](#4-file-trên-máy-đích)
5. [Môi trường phát triển và kiểm thử](#5-môi-trường-phát-triển-và-kiểm-thử)
6. [Cách đọc một task](#6-cách-đọc-một-task)
7. [Danh sách task](#7-danh-sách-task)
   - [P: Chuẩn bị](#p-chuẩn-bị)
   - [C: libcommon](#c-libcommon)
   - [CAM: camera-service](#cam-camera-service)
   - [STR: streaming-service](#str-streaming-service)
   - [SYS: system-service](#sys-system-service)
   - [CLD: cloud-service](#cld-cloud-service)
   - [DEV: device-service](#dev-device-service)
   - [AI: ai-service](#ai-ai-service)
   - [PKG: Đóng gói và triển khai](#pkg-đóng-gói-và-triển-khai)
8. [Kiểm thử tích hợp toàn hệ thống](#8-kiểm-thử-tích-hợp-toàn-hệ-thống)
9. [Nhật ký tiến độ](#9-nhật-ký-tiến-độ)

---

## 1. Kiến trúc mới

Một thư viện chung và 6 service. Mỗi service lo trọn một mảng chức năng, chạy độc lập bằng systemd, và một service
chết không kéo các service khác chết theo.

| # | Service (thư mục) | Chương trình | Lo việc gì | Tương ứng V2 |
|---|---|---|---|---|
| 0 | `Common/` | `libeyerus-common.a` | Log, config, ghi file nguyên tử, chạy lệnh, crypto, IPC, khung `main` | `App/System`, `Server/Settings`, `Server/Crypto` |
| 1 | `CameraService/` | `eyerus-camera` | Quét ONVIF, quản lý camera, lấy RTSP URI, PTZ, webcam USB | `App/Camera`, `App/Api/PtzService` |
| 2 | `StreamingService/` | `eyerus-streaming` | Pipeline GStreamer chuyển tiếp RTSP (không giải mã), tự kết nối lại, nhánh khung hình cho AI | `App/Stream` |
| 3 | `SystemService/` | `eyerus-system` | REST API + Web UI, thông tin / điều khiển máy, mạng, WiFi, router, firewall | `App/Api`, `App/System`, `StartUp/Router`, `StartUp/Wifi` |
| 4 | `CloudService/` | `eyerus-cloud` | Commadesk: enroll, ký HMAC, MQTT, telemetry, lệnh từ xa, đồng bộ giờ, OTA | `Server/` |
| 5 | `DeviceService/` | `eyerus-device` | Phần cứng tại chỗ: GPIO (nút, LED), cảm biến, Bluetooth (Improv, SPP), factory reset | `StartUp/Ble`, `Classic`, `Gpio`, `Provision`, `Sensor/` |
| 6 | `AIService/` | `eyerus-ai` | Nhận khung hình fps thấp, phân tích, sinh sự kiện | mới |

Có 2 nguyên tắc:

- **Một tài nguyên chỉ có một chủ.** GPIO và Bluetooth thuộc device-service, `nmcli`/`nft` thuộc system-service,
  MQTT thuộc cloud-service. Service nào cần dùng tài nguyên của service khác thì gửi lệnh qua IPC.
- **Một bộ lệnh dùng chung cho mọi kênh.** REST, MQTT và Bluetooth đều chuyển thành lệnh IPC giống nhau, nên kết quả
  trả về giống nhau.

```
 Trình duyệt ──REST──► eyerus-system ─────┐
 Server ──MQTT──► eyerus-cloud ───────────┤   (IPC: Unix socket, 1 dòng JSON / lệnh)
 Điện thoại ──BLE/SPP──► eyerus-device ───┘
        │
        ▼
 camera.sock   streaming.sock   system.sock   device.sock   cloud.sock   ai.sock

 eyerus-camera ──(RTSP URI qua IPC)──► eyerus-streaming ──rtspclientsink──► Media server
                                             │ tee: decode → 2 fps → shmsink
                                             ▼
                                         eyerus-ai

 Trạng thái: mỗi service ghi file JSON nguyên tử vào /run/eyerus/, ai cần thì đọc
 (cloud-service gửi telemetry, system-service hiển thị trên web).
```

---

## 2. Quy ước chung

### Cấu trúc một service

```
XxxService/
├── CMakeLists.txt
├── Inc/Xxx/...          header (namespace xxx)
├── Src/                 .cpp; Src/main.cpp chỉ ghép các lớp lại
├── Config/              file cấu hình mẫu
├── Test/                unit test GoogleTest (test_*.cpp)
├── Scripts/             script kiểm thử thủ công / e2e (*.sh, *.py)
└── eyerus-xxx.service   unit systemd
```

### Code

- C++17, build với `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror`.
- Không dùng exception để báo lỗi nghiệp vụ. Hàm có thể lỗi trả `common::Status` hoặc `common::Result<T>` (task C-02).
- Tên: lớp `PascalCase`, hàm và biến `camelCase`, thành viên `name_`, hằng `kName`.
- Lớp nào cần test mà không có phần cứng thì tách thành 2 phần: **phần thuần** (nhận dữ liệu, trả kết quả, không đụng
  hệ thống) và **phần thực thi** (gọi socket, GPIO, `nmcli`, ...). Phần thuần phải có unit test. Đây là cách V2 làm với
  `RouterPlanner` và `ButtonLogic`.
- Log ra `stderr` (journald tự thu). Không in mật khẩu: dùng `maskSecret()` (task C-01).

### Build và test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2          # board yếu: luôn -j2
ctest --test-dir build --output-on-failure
```

Unit test dùng GoogleTest + CTest. Kiểm thử tích hợp dùng script trong `Scripts/`, chạy tay hoặc gọi từ CTest với
label `e2e` (`ctest -L e2e`).

### Git

Mỗi task là 1 commit, message bắt đầu bằng mã task, ví dụ `CAM-05: WS-Discovery probe`.

---

## 3. Giao thức IPC giữa các service

**Kênh truyền:** Unix domain socket kiểu stream, mỗi service một socket `/run/eyerus/<tên>.sock` (quyền `660`,
nhóm `eyerus`). Mỗi bản tin là **một dòng JSON** kết thúc bằng `\n`, dài tối đa 64 KiB. Một kết nối gửi được nhiều
lệnh nối tiếp nhau.

**Yêu cầu:**

```json
{"id":"42","cmd":"camera.get","args":{"id":"cam01"},"source":"rest"}
```

- `id`: chuỗi do bên gọi đặt, bên trả lời gửi lại y nguyên.
- `cmd`: `<miền>.<hành động>`, ví dụ `camera.list`, `stream.start`.
- `args`: object, có thể bỏ trống.
- `source`: kênh gốc của lệnh (`rest`, `mqtt`, `ble`, `cli`, `internal`). Dùng để ghi log và phân quyền.

**Trả lời:**

```json
{"id":"42","ok":true,"data":{...}}
{"id":"42","ok":false,"error":{"code":"not_found","message":"camera cam09 không tồn tại"}}
```

**Mã lỗi chung:** `bad_json`, `bad_request`, `unknown_command`, `not_found`, `conflict`, `busy`, `timeout`,
`unavailable` (service đích không chạy), `internal`.

**Lệnh bắt buộc ở mọi service:** `ping` trả `{"service":"camera","version":"0.1.0","uptime_s":12}`.

**Thử bằng tay:**

```bash
echo '{"id":"1","cmd":"ping"}' | socat - UNIX-CONNECT:/run/eyerus/camera.sock
```

**Ánh xạ REST sang IPC** (system-service làm): `GET /api/cameras` → `camera.list`, `POST /api/cameras` →
`camera.add`, `GET /api/cameras/{id}` → `camera.get`, ... Ánh xạ HTTP status: `not_found`→404, `bad_request`→400,
`conflict`→409, `busy`→409, `unavailable`→503, `timeout`→504, còn lại→500.

**Ánh xạ MQTT sang IPC** (cloud-service làm): bản tin trên `.../commands` có `command` → `cmd`, `params` → `args`.
Kết quả gửi lên `.../command-ack`.

---

## 4. File trên máy đích

| Đường dẫn | Ai ghi | Nội dung |
|---|---|---|
| `/etc/eyerus/cameras.json` | camera (600) | Danh sách camera, có mật khẩu |
| `/etc/eyerus/streams.json` | streaming (600) | Danh sách luồng |
| `/etc/eyerus/auth.json` | system (600) | Tài khoản REST, mật khẩu băm PBKDF2 |
| `/etc/eyerus/router.json` | system | LAN, DHCP, firewall |
| `/etc/eyerus/cloud.json`, `identity.json` | cloud (600) | Broker, topic, chu kỳ; secret enroll |
| `/etc/eyerus/device.json` | device | Chân GPIO, cảm biến, Bluetooth |
| `/run/eyerus/*.sock` | từng service | Socket IPC |
| `/run/eyerus/status/camera.json` | camera | Trạng thái camera (online / lỗi) |
| `/run/eyerus/status/streams.json` | streaming, 2 s | Trạng thái luồng: state, frames, bytes, retry, lỗi |
| `/run/eyerus/status/sensors.json` | device, 2 s | Nhiệt độ, độ ẩm, AQI |
| `/run/eyerus/status/cloud.json` | cloud, 1 s | Trạng thái MQTT, liên kết |
| `/run/eyerus/status/ai-events.json` | ai | Sự kiện gần nhất |
| `/var/lib/eyerus/ota/` | cloud | Gói OTA, `state.json` |
| `/var/lib/eyerus/device/setup-mode` | device | Cờ setup mode |

File trạng thái luôn có trường `updated_at` (UTC). Bên đọc thấy file cũ hơn **15 s** thì coi bên ghi đã dừng.

---

## 5. Môi trường phát triển và kiểm thử

### Gói cần cài (Ubuntu)

```bash
sudo apt install -y build-essential cmake pkg-config git \
  libgtest-dev nlohmann-json3-dev libssl-dev libasio-dev libpugixml-dev \
  libcurl4-openssl-dev libmosquitto-dev libsystemd-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-rtsp \
  socat jq curl mosquitto mosquitto-clients ffmpeg v4l-utils gpiod \
  python3 python3-paho-mqtt docker.io
```

### Đồ giả lập để test không cần phần cứng

| Thứ cần | Giả bằng | Ghi chú |
|---|---|---|
| Camera RTSP | **mediamtx** (Docker) + `ffmpeg`/`gst-launch` đẩy `videotestsrc` vào | Xem P-03 |
| Camera ONVIF | Lớp `FakeCamera` trong `eyerus/Tools/tests/test_camera_ptz_e2e.py` | Tách ra `Tools/fake_onvif.py` (P-04) |
| Media server nhận luồng | Chính mediamtx (path khác, ví dụ `/out/cam01`) | |
| Broker MQTT | `mosquitto` chạy local, cổng 1883 | `mosquitto_sub -v -t '#'` để xem |
| Server Commadesk (HTTP) | Script Python `Tools/fake_commadesk.py` (CLD-06) | Kiểm tra chữ ký HMAC |
| Webcam USB | `v4l2loopback` + `ffmpeg -f lavfi -i testsrc` | |
| GPIO | Kernel 5.15: module `gpio-mockup` (`gpio-sim` cần kernel ≥ 5.17) | Ngoài ra test trên board thật |
| Bluetooth | Board thật + app nRF Connect / Serial Bluetooth Terminal; xem gói tin bằng `btmon` | |
| Router / firewall | Network namespace (`ip netns`) + `nft` thật | Tham khảo `eyerus/Tools/tests/test_firewall_e2e.py` |

---

## 6. Cách đọc một task

```
### CAM-05 Tên task                       ← mã: tiền tố service + số thứ tự
- [ ] trạng thái
- Phụ thuộc: task phải xong trước
- Làm: các bước code
- Kiểm thử: unit test (U) và kiểm thử tay / tích hợp (T); phải đạt hết
- Xong khi: tiêu chí chốt
- Tham khảo V2: file nên đọc trước khi code
```

Thứ tự đề xuất: **P → C → CAM → STR → SYS → CLD → DEV → PKG → AI**. Từ SYS trở đi có thể đổi thứ tự, miễn đủ phụ
thuộc. PKG-01 (unit systemd) có thể làm sớm ngay sau CAM để quen với cách chạy thật.

---

## 7. Danh sách task

### P: Chuẩn bị

#### P-01 Khung repo và CMake gốc
- [ ] trạng thái
- Làm:
  - `git init`, `.gitignore` (`build/`, `*.user`).
  - `CMakeLists.txt` gốc: `add_subdirectory(Common)` và từng service; tuỳ chọn `EYERUS_BUILD_TESTS` (mặc định ON).
  - Mỗi service tạm có `Src/main.cpp` in ra tên rồi thoát.
  - Bật CTest (`enable_testing()`), tìm GoogleTest bằng `find_package(GTest REQUIRED)`.
- Kiểm thử:
  - (T) `cmake -S . -B build && cmake --build build -j2` không có cảnh báo.
  - (T) `./build/CameraService/eyerus-camera` in ra tên.
  - (T) `ctest --test-dir build` chạy được (0 test cũng được).
- Xong khi: build sạch từ thư mục `build/` trống.

#### P-02 Cài công cụ và kiểm tra
- [ ] trạng thái
- Làm: cài gói ở mục 5.
- Kiểm thử:
  - (T) `gst-inspect-1.0 rtspclientsink` và `gst-inspect-1.0 rtspsrc` đều có.
  - (T) `mosquitto -h`, `socat -V`, `jq --version` chạy được.
  - (T) Trong một test GoogleTest mẫu `EXPECT_EQ(1, 1)` chạy qua `ctest`.
- Xong khi: mọi lệnh trên chạy được.

#### P-03 Nguồn RTSP giả
- [ ] trạng thái
- Làm: viết `Tools/rtsp_fake.sh start|stop`:
  - Chạy `docker run -d --name eyerus-mtx -p 8554:8554 bluenviron/mediamtx:latest`.
  - Đẩy nguồn H.264 vào `rtsp://localhost:8554/cam01`:
    `ffmpeg -re -f lavfi -i testsrc=size=1280x720:rate=25 -c:v libx264 -preset ultrafast -tune zerolatency -f rtsp rtsp://localhost:8554/cam01`
  - Thêm nguồn H.265 `cam02` (`-c:v libx265`) và nguồn có tiếng `cam03` (`-f lavfi -i sine` + `-c:a aac`).
- Kiểm thử:
  - (T) `ffprobe rtsp://localhost:8554/cam01` thấy `h264 1280x720`.
  - (T) `gst-launch-1.0 rtspsrc location=rtsp://localhost:8554/cam01 ! fakesink` chạy không lỗi 10 s.
- Xong khi: có 3 nguồn giả bật/tắt bằng 1 lệnh.

#### P-04 Camera ONVIF giả
- [ ] trạng thái
- Làm: tách `FakeCamera` từ `eyerus/Tools/tests/test_camera_ptz_e2e.py` thành `Tools/fake_onvif.py`:
  - Trả lời WS-Discovery trên UDP 3702.
  - Kiểm tra WS-Security, trả `GetProfiles`, `GetStreamUri` trỏ tới mediamtx (P-03).
  - Ghi lại các lệnh PTZ nhận được.
- Kiểm thử:
  - (T) Chạy script, dùng ONVIF Device Manager (Windows) hoặc `python3 -c` gửi Probe, thấy trả lời.
- Xong khi: chạy `python3 Tools/fake_onvif.py --user admin --password admin` là có camera ảo.

---

### C: libcommon

Thư mục `Common/`, namespace `common`. Mọi task có unit test trong `Common/Test/`.

#### C-01 Logger
- [ ] trạng thái
- Làm: `log::info/warn/error/debug(fmt...)`; mức log đặt lúc chạy; mỗi dòng có tiền tố mức theo chuẩn sd-daemon
  (`<6>` info, `<4>` warn, `<3>` error) để journald nhận đúng mức; `maskSecret("Admin123")` → `"Ad******"`.
  Ghi log phải thread-safe.
- Kiểm thử:
  - (U) Mức `warn` thì `info` không in; định dạng dòng đúng mẫu.
  - (U) `maskSecret` với chuỗi rỗng, 1 ký tự, 10 ký tự.
  - (U) 8 thread cùng log 1000 dòng: không dòng nào bị trộn (đếm đủ 8000 dòng nguyên vẹn).
- Xong khi: test xanh.

#### C-02 Status và Result
- [ ] trạng thái
- Làm: `Status{ok, code, message}` với `code` là chuỗi theo mã lỗi IPC (mục 3); `Result<T>` chứa giá trị hoặc lỗi;
  hàm tiện ích `Status::error("not_found", "...")`.
- Kiểm thử: (U) tạo ok / lỗi; lấy giá trị từ Result lỗi thì bị assert (test `EXPECT_DEATH` ở build Debug).
- Xong khi: test xanh.

#### C-03 Ghi file nguyên tử
- [ ] trạng thái
- Làm: `writeFileAtomic(path, content, mode)`:
  - Ghi vào `path.tmp.<pid>` cùng thư mục.
  - `fchmod(mode)`, `fsync(file)`.
  - `rename()` đè lên file đích, rồi `fsync` thư mục chứa.
  - Lỗi thì xoá file tạm.

  Thêm `readFile(path)` trả `Result<std::string>`.
- Kiểm thử:
  - (U) Ghi rồi đọc lại đúng nội dung; `stat` thấy quyền `0600`.
  - (U) Sau khi ghi không còn file `*.tmp.*`.
  - (U) Ghi vào thư mục không tồn tại → lỗi, không crash.
  - (U) Một thread ghi liên tục 2 nội dung A/B 1000 lần, một thread đọc liên tục: lần đọc nào cũng ra đúng A hoặc B,
    không bao giờ ra file rỗng hay nửa vời.
- Xong khi: test xanh.
- Tham khảo V2: `Server/Settings/RuntimeFiles.cpp`.

#### C-04 Đọc và kiểm tra config JSON
- [ ] trạng thái
- Phụ thuộc: C-02, C-03
- Làm: lớp `JsonReader` có `requireString("id")`, `optionalInt("latency_ms", 200, min, max)`, ...; gom **mọi** lỗi
  thành danh sách có đường dẫn (`cameras[2].url: phải là chuỗi`) thay vì dừng ở lỗi đầu tiên.
- Kiểm thử:
  - (U) Dùng đúng file `CameraService/Config/Cameras.json` hiện tại (đang lỗi): phải báo đủ 3 lỗi: `cam01` trùng,
    `cameras[2].url` sai kiểu, `cameras[3]` thiếu `rtsp_url`.
  - (U) File không phải JSON → lỗi có số dòng / cột.
- Xong khi: test xanh.

#### C-05 Chạy lệnh ngoài (CommandRunner)
- [ ] trạng thái
- Làm: `runCommand({"nmcli","-t","device"}, timeout)`:
  - Dùng `posix_spawnp` + pipe, không qua shell.
  - Thu stdout/stderr, trả mã thoát; quá giờ thì `SIGTERM`, sau 2 s thì `SIGKILL`.
  - Đặt `O_CLOEXEC` cho mọi fd tự mở. V2 bị rò socket sang tiến trình con vì thiếu cờ này.
- Kiểm thử:
  - (U) `echo hi` → stdout `hi\n`, mã 0.
  - (U) `false` → mã 1.
  - (U) `sleep 10` với timeout 1 s → bị kill trong khoảng 1–3 s, báo `timeout`.
  - (U) Lệnh không tồn tại → lỗi rõ ràng.
  - (U) `echo '$(id)'` in nguyên chữ `$(id)` (chứng minh không qua shell).
  - (U) Mở 1 socket rồi chạy `ls -l /proc/self/fd`: tiến trình con không thấy socket đó.
- Xong khi: test xanh.
- Tham khảo V2: `App/System/CommandRunner.cpp`.

#### C-06 SHA-256
- [ ] trạng thái
- Làm: tự viết theo FIPS 180-4; có API cộng dồn (`update`/`final`) để băm file lớn.
- Kiểm thử:
  - (U) Vector chuẩn: `""` → `e3b0c442...b855`, `"abc"` → `ba7816bf...15ad`, 1.000.000 chữ `a` → `cdc76e5c...0d0`.
  - (U) Băm file 50 MB ngẫu nhiên, so với `sha256sum`.
  - (U) Gọi `update` theo từng mẩu kích thước ngẫu nhiên vẫn ra cùng kết quả.
- Xong khi: test xanh.
- Tham khảo V2: `Server/Crypto/Sha256.cpp`.

#### C-07 HMAC-SHA256
- [ ] trạng thái
- Phụ thuộc: C-06
- Làm: theo RFC 2104; khoá dài hơn 64 byte thì băm khoá trước. Thêm `constantTimeEquals`.
- Kiểm thử: (U) RFC 4231 test case 1, 2, 3, 4, 6, 7 (case 6 và 7 dùng khoá 131 byte).
- Xong khi: test xanh.

#### C-08 Base64
- [ ] trạng thái
- Làm: encode/decode chuẩn RFC 4648 (có padding); decode chuỗi sai trả lỗi.
- Kiểm thử:
  - (U) Vector RFC: `""`, `f`→`Zg==`, `fo`→`Zm8=`, `foo`→`Zm9v`, `foobar`→`Zm9vYmFy`.
  - (U) Dữ liệu nhị phân ngẫu nhiên: decode(encode(x)) == x.
  - (U) `"Zm9v!"`, `"Zg="` → lỗi.
- Xong khi: test xanh.

#### C-09 UUID và thời gian
- [ ] trạng thái
- Làm: `newUuidV4()` (lấy byte ngẫu nhiên từ `getrandom()`); `utcNowIso()` → `2026-11-14T08:30:00.123Z`;
  `parseIso()`.
- Kiểm thử:
  - (U) UUID khớp regex `^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$`.
  - (U) 100.000 UUID không trùng.
  - (U) parse(format(t)) == t.
- Xong khi: test xanh.

#### C-10 IPC server
- [ ] trạng thái
- Phụ thuộc: C-01, C-02
- Làm: `IpcServer(path)`:
  - Socket `AF_UNIX/SOCK_STREAM`; xoá file socket cũ khi khởi động; quyền `660`.
  - Một thread dùng `poll()` cho nhiều client; tách dòng theo `\n`; dòng dài hơn 64 KiB thì đóng kết nối.
  - `on("camera.list", handler)`; tự có lệnh `ping`.
  - JSON hỏng → `bad_json`; lệnh lạ → `unknown_command`.
  - Handler chạy lâu không được chặn client khác: giao cho thread pool nhỏ (2–4 thread).
- Kiểm thử:
  - (T) `echo '{"id":"1","cmd":"ping"}' | socat - UNIX-CONNECT:/tmp/t.sock` → `ok:true`.
  - (U) Gửi `abc\n` → `bad_json`; `{"id":"2","cmd":"x"}` → `unknown_command` và `id` trả về đúng `"2"`.
  - (U) 2 lệnh trong 1 lần gửi (`...\n...\n`) → 2 trả lời đúng thứ tự `id`.
  - (U) Một handler `sleep 2 s`, client khác `ping` vẫn có trả lời dưới 100 ms.
  - (U) 50 client cùng lúc, mỗi client 100 lệnh: đủ 5000 trả lời.
  - (U) Gửi 100 KiB không có `\n` → server đóng kết nối, server vẫn sống.
- Xong khi: test xanh.

#### C-11 IPC client
- [ ] trạng thái
- Phụ thuộc: C-10
- Làm: `IpcClient::call(socketPath, cmd, args, timeout)` → `Result<json>`. Tự sinh `id`; kết nối không được thì trả
  `unavailable`; quá giờ thì trả `timeout`.
- Kiểm thử:
  - (U) Gọi `ping` tới IpcServer chạy trong cùng tiến trình.
  - (U) Socket không tồn tại → `unavailable` ngay lập tức.
  - (U) Handler ngủ 3 s, timeout 1 s → `timeout` trong khoảng 1–1.2 s.
- Xong khi: test xanh.

#### C-12 Khung service (ServiceMain)
- [ ] trạng thái
- Phụ thuộc: C-01, C-10
- Làm: hàm dùng chung cho mọi `main()`:
  - Tham số `--config <dir>`, `--run-dir <dir>`, `--log-level`, `--help`, `--version`. `--run-dir` giúp chạy dev
    trong `build/run` thay vì `/run`.
  - Bắt `SIGTERM`/`SIGINT` bằng `signalfd` (hoặc self-pipe), dừng IPC server, xoá file socket.
  - Gọi `sd_notify("READY=1")` khi sẵn sàng (cho unit `Type=notify`).
- Kiểm thử:
  - (T) `--help` in hướng dẫn, mã 0; tham số sai → mã 2.
  - (T) Chạy service, `kill -TERM <pid>` → thoát mã 0 trong dưới 1 s, file socket bị xoá.
  - (T) `kill -9` rồi chạy lại → vẫn khởi động được (xoá socket cũ).
- Xong khi: CameraService rỗng chạy bằng khung này trả lời `ping`.

#### C-13 Theo dõi file đổi (FileWatcher)
- [ ] trạng thái
- Làm: báo khi file đổi. Dùng `inotify` trên **thư mục** chứa file (vì `rename` thay inode), hoặc so `mtime` mỗi
  giây.
- Kiểm thử:
  - (U) Sửa file bằng `writeFileAtomic` → callback được gọi đúng 1 lần.
  - (U) Xoá rồi tạo lại file → vẫn được báo.
- Xong khi: test xanh. Dùng ở CLD-02 và SYS.

#### C-14 Đọc file trạng thái (StatusReader)
- [ ] trạng thái
- Phụ thuộc: C-03, C-09
- Làm: `readStatus(path, maxAge=15s)` → `{data, stale: bool}` dựa trên `updated_at`.
- Kiểm thử: (U) file mới → `stale=false`; `updated_at` lùi 20 s → `stale=true`; file không có → lỗi `not_found`.
- Xong khi: test xanh.

---

### CAM: camera-service

`CameraService/`, chương trình `eyerus-camera`, socket `camera.sock`.

Lệnh IPC: `camera.list`, `camera.get`, `camera.add`, `camera.update`, `camera.remove`, `camera.scan`,
`camera.check`, `camera.profiles`, `camera.stream_uri`, `camera.ptz.move`, `camera.ptz.stop`,
`camera.ptz.presets`, `camera.ptz.goto`, `camera.ptz.save`, `camera.ptz.remove`, `camera.usb.list`.

#### CAM-00 Sắp xếp lại khung hiện có
- [ ] trạng thái
- Làm:
  - Chuyển `FrameData`, `FrameCallback`, `GstPipeline` sang `StreamingService` (camera-service không giải mã hình).
  - Đổi `ICameraService` thành `ICamera` với các hàm `describe()`, `getProfiles()`, `getStreamUri(profile)`,
    `ptz()` (có thể trả null).
  - Sửa `Config/Cameras.json` theo mẫu:
    ```json
    {"cameras":[{"id":"cam01","name":"Cổng trước","type":"rtsp","rtsp_url":"rtsp://192.168.1.108:554/cam/realmonitor?channel=1&subtype=0","username":"admin","password":"..."}]}
    ```
- Kiểm thử: (T) build sạch.
- Xong khi: không còn code GStreamer trong CameraService.

#### CAM-01 Mô hình Camera và lưu trữ
- [ ] trạng thái
- Phụ thuộc: C-03, C-04
- Làm:
  - `CameraInfo{id, name, type: rtsp|onvif|usb, host, port, username, password, onvif{device_url, media_url, ptz_url}, profiles[], selected_profile, rtsp_url, usb{device, format, width, height, fps}}`.
  - `CameraStore::load/save` vào `cameras.json` (quyền 600).
  - `id` chỉ cho phép `[a-z0-9-]{1,32}`.
- Kiểm thử:
  - (U) save → load ra đúng như cũ (so từng trường).
  - (U) `id` trùng hoặc `id` có dấu cách → lỗi.
  - (U) Không có file → danh sách rỗng, không lỗi.
- Xong khi: test xanh.

#### CAM-02 CameraRegistry (thread-safe)
- [ ] trạng thái
- Phụ thuộc: CAM-01
- Làm: add/update/remove/get/list có mutex; mỗi lần đổi thì lưu xuống đĩa; `list()` trả bản sao và che mật khẩu.
- Kiểm thử:
  - (U) 4 thread cùng thêm 100 camera khác nhau → đủ 400, file trên đĩa cũng đủ 400.
  - (U) Kết quả `list()` không chứa mật khẩu thật.
- Xong khi: test xanh.

#### CAM-03 Lệnh IPC quản lý camera
- [ ] trạng thái
- Phụ thuộc: CAM-02, C-12
- Làm: `camera.list/get/add/update/remove`; kiểm tra `args` và báo lỗi `bad_request` có tên trường.
- Kiểm thử: (T) script `CameraService/Scripts/test_crud.sh` dùng socat + jq:
  1. add `cam01` → ok
  2. add `cam01` lần nữa → `conflict`
  3. get → có dữ liệu, mật khẩu bị che
  4. update name → ok
  5. restart service → `cam01` vẫn còn, tên mới
  6. remove → ok; get → `not_found`
- Xong khi: script in `PASS` cho cả 6 bước.

#### CAM-04 Xử lý URL RTSP
- [ ] trạng thái
- Làm: `RtspUrl::parse/format`; ghép `username`/`password` vào URL có percent-encoding; `redact(url)` để ghi log.
- Kiểm thử:
  - (U) Mật khẩu `a@b:c/d` → `rtsp://admin:a%40b%3Ac%2Fd@host:554/path`.
  - (U) Có IPv6 `[fe80::1]:554` → parse đúng.
  - (U) `redact` → `rtsp://admin:***@host/...`.
  - (U) URL đã có user/pass và có thêm username riêng → lấy username riêng.
- Xong khi: test xanh.

#### CAM-05 WS-Discovery
- [ ] trạng thái
- Phụ thuộc: P-04
- Làm:
  - Gửi SOAP `Probe` (Types `dn:NetworkVideoTransmitter`, `MessageID` = UUID) qua UDP multicast
    `239.255.255.250:3702`, gửi trên **từng** card mạng IPv4 (`IP_MULTICAST_IF`).
  - Nhận `ProbeMatch` trong N giây; parse bằng pugixml (bỏ qua prefix namespace); lấy `XAddrs`, `Scopes` (tên, hãng);
    bỏ trùng theo địa chỉ.
- Kiểm thử:
  - (U) Parse file mẫu `Test/fixtures/probe_match_*.xml`: 1 file Dahua, 1 file Hikvision, 1 file Tapo. Có thể lấy
    trong V2 hoặc bắt từ camera thật bằng `tcpdump -A udp port 3702`.
  - (T) Chạy `fake_onvif.py` → `camera.scan` thấy 1 camera.
  - (T) Trong LAN thật thấy đủ camera mà ONVIF Device Manager thấy.
- Xong khi: test xanh và thấy camera thật.
- Tham khảo V2: `App/Camera/OnvifCamera.cpp` (`buildProbe`).

#### CAM-06 SOAP client
- [ ] trạng thái
- Làm: `SoapClient::call(url, action, bodyXml, timeout)` dùng Asio TCP: tự viết HTTP POST,
  `Content-Type: application/soap+xml; charset=utf-8; action="..."`; đọc header + body theo `Content-Length` hoặc
  `chunked`; trả `SOAP Fault` thành lỗi có `Reason`.
- Kiểm thử:
  - (U) Parse câu trả lời `chunked` mẫu.
  - (T) URL tới `nc -l 9999` không trả lời → `timeout` đúng giờ.
  - (T) Fault mẫu (`ter:NotAuthorized`) → lỗi `unauthorized`.
- Xong khi: test xanh.

#### CAM-07 WS-Security UsernameToken
- [ ] trạng thái
- Phụ thuộc: C-08, OpenSSL SHA1
- Làm: `PasswordDigest = Base64(SHA1(nonce_raw + created + password))`; nonce 16 byte ngẫu nhiên;
  `created` = UTC hiện tại. Có tuỳ chọn trừ lệch giờ: lấy giờ camera bằng `GetSystemDateAndTime` (không cần đăng
  nhập), vì nhiều camera từ chối khi lệch giờ.
- Kiểm thử:
  - (U) Vector cố định, tự tính bằng Python rồi chép vào test:
    ```bash
    python3 -c "import hashlib,base64;n=bytes(range(16));c='2026-01-01T00:00:00Z';print(base64.b64encode(hashlib.sha1(n+c.encode()+b'admin').digest()).decode())"
    ```
  - (T) `fake_onvif.py` chấp nhận đúng mật khẩu, từ chối mật khẩu sai.
- Xong khi: test xanh.

#### CAM-08 Lấy thông tin ONVIF
- [ ] trạng thái
- Phụ thuộc: CAM-06, CAM-07
- Làm: `GetDeviceInformation`, `GetServices` (lấy URL Media/PTZ, **thay host trong URL bằng IP thật** vì nhiều camera
  trả IP nội bộ sai), `GetProfiles` (token, tên, codec, độ phân giải), `GetStreamUri` (RTP-Unicast, RTSP).
- Kiểm thử:
  - (U) Parse fixture XML trả lời của từng lệnh.
  - (U) URL dịch vụ `http://0.0.0.0/onvif/media` thay host thành `192.168.1.108`.
  - (T) Với `fake_onvif.py` và với 1 camera thật: profile và URI khớp ONVIF Device Manager.
- Xong khi: test xanh.
- Tham khảo V2: `Tools/tests/test_onvif_service_url.py`.

#### CAM-09 Luồng thêm camera ONVIF
- [ ] trạng thái
- Phụ thuộc: CAM-03, CAM-05, CAM-08
- Làm: `camera.scan` (trả danh sách tìm thấy, đánh dấu cái đã thêm); `camera.add` với `type=onvif` sẽ đăng nhập,
  lấy profile, chọn profile chính, lưu `rtsp_url`. Sai mật khẩu → `bad_request` + `unauthorized`.
- Kiểm thử: (T) script: scan → add bằng mật khẩu sai (lỗi) → add bằng mật khẩu đúng (ok, có 2 profile) →
  `camera.stream_uri` ra URL mediamtx; `ffprobe` URL đó chạy được.
- Xong khi: script PASS.

#### CAM-10 Kiểm tra camera (camera.check)
- [ ] trạng thái
- Làm: thử TCP tới cổng RTSP, gửi `OPTIONS` và `DESCRIBE` (tự viết RTSP tối thiểu, có Digest auth) để biết:
  online / sai mật khẩu (401) / sai đường dẫn (404). Ghi kết quả vào `status/camera.json` mỗi 30 s.
- Kiểm thử:
  - (T) mediamtx bật → `online`; tắt → `offline`.
  - (T) mediamtx bật auth, sai mật khẩu → `unauthorized`.
  - (T) Path sai → `not_found`.
- Xong khi: 3 trường hợp đúng.

#### CAM-11 PTZ
- [ ] trạng thái
- Phụ thuộc: CAM-08
- Làm: `ContinuousMove(pan, tilt, zoom ∈ [-1,1], timeout)`: **Timeout làm tròn lên giây** (`PT2S`), tự gửi
  `Stop` khi hết `timeout_ms` (dùng timer). Thêm `Stop`, `GetPresets`, `GotoPreset`, `SetPreset`, `RemovePreset`.
  Hàng đợi riêng cho PTZ để lệnh dồn dập không chặn IPC.
- Kiểm thử:
  - (U) `formatDuration(1500ms)` → `PT2S`.
  - (T) `fake_onvif.py` ghi nhận Move rồi Stop sau đúng khoảng `timeout_ms`.
  - (T) Camera thật (Tapo C200): pan trái/phải, lưu và gọi preset.
- Xong khi: chạy được trên camera thật.
- Tham khảo V2: `App/Api/PtzService.cpp`, `Tools/tests/test_camera_ptz_e2e.py`.

#### CAM-12 Webcam USB (V4L2)
- [ ] trạng thái
- Làm: quét `/dev/video*` bằng `VIDIOC_QUERYCAP` (chỉ lấy thiết bị có `V4L2_CAP_VIDEO_CAPTURE`), `VIDIOC_ENUM_FMT`,
  `VIDIOC_ENUM_FRAMESIZES`, `VIDIOC_ENUM_FRAMEINTERVALS` → `camera.usb.list`.
- Kiểm thử:
  - (T) `sudo modprobe v4l2loopback`, `ffmpeg -f lavfi -i testsrc -f v4l2 /dev/video10` → thấy thiết bị.
  - (T) So kết quả với `v4l2-ctl --list-formats-ext -d /dev/video10`.
- Xong khi: kết quả khớp `v4l2-ctl`.

---

### STR: streaming-service

`StreamingService/`, chương trình `eyerus-streaming`, socket `streaming.sock`.

Lệnh IPC: `stream.list`, `stream.get`, `stream.add`, `stream.update`, `stream.remove`, `stream.start`,
`stream.stop`, `stream.restart`, `stream.zoom` (cho webcam).

#### STR-01 Thử pipeline bằng gst-launch
- [ ] trạng thái
- Phụ thuộc: P-03
- Làm: viết `Scripts/relay_proto.sh` chuyển tiếp không giải mã:
  ```bash
  gst-launch-1.0 rtspsrc location=rtsp://localhost:8554/cam01 latency=200 protocols=tcp ! \
    rtph264depay ! h264parse config-interval=-1 ! queue ! \
    rtspclientsink location=rtsp://localhost:8554/out/cam01 protocols=tcp
  ```
  Làm thêm bản H.265 (`rtph265depay ! h265parse`).
- Kiểm thử:
  - (T) `ffprobe rtsp://localhost:8554/out/cam01` → `h264 1280x720`.
  - (T) `top`: CPU của gst-launch dưới 5% (chứng minh không giải mã).
  - (T) Mở bằng VLC thấy hình.
- Xong khi: chạy ổn định 10 phút.

#### STR-02 Mô hình Stream và lưu trữ
- [ ] trạng thái
- Phụ thuộc: C-03, C-04
- Làm: `StreamConfig{id, camera_id | source_url, profile, server_url, enabled, latency_ms, audio: auto|on|off, ai: {enabled, fps}}`,
  `StreamStore` lưu `streams.json`.
- Kiểm thử: (U) save/load; phải có đúng 1 trong 2 trường `camera_id` / `source_url`; `server_url` phải là `rtsp://`.
- Xong khi: test xanh.

#### STR-03 Lớp RelayPipeline bằng C API
- [ ] trạng thái
- Phụ thuộc: STR-01
- Làm:
  - Tạo element bằng `gst_element_factory_make`. Bắt `pad-added` của `rtspsrc` (pad sinh động).
  - Đọc caps `encoding-name` (H264/H265) để chọn depay/parse, rồi link.
  - Chạy `GMainLoop` ở thread riêng cho mọi pipeline (một loop dùng chung).
  - `start()`, `stop()` đưa về `GST_STATE_NULL` và giải phóng hết (không rò ref).
- Kiểm thử:
  - (T) Chạy với `cam01` (H.264) và `cam02` (H.265): `ffprobe` đầu ra đúng codec.
  - (T) start/stop 100 lần liên tiếp: RSS (xem `ps -o rss`) không tăng quá 5 MB.
  - (T) `GST_DEBUG=2` không có `WARN` lạ khi chạy bình thường.
- Xong khi: test xanh.
- Tham khảo V2: `App/Stream/RtspStreamPusher.cpp`.

#### STR-04 Bus và bộ đếm
- [ ] trạng thái
- Phụ thuộc: STR-03
- Làm: `gst_bus_add_watch`: xử lý `ERROR` (lưu `last_error`), `EOS`, `STATE_CHANGED` của pipeline. Pad probe trên
  src pad của parser để đếm `frames`, `bytes` và tính bitrate (kbps, cửa sổ 5 s).
- Kiểm thử:
  - (T) Đang chạy thì dừng mediamtx → nhận `ERROR`, state chuyển `error`.
  - (T) Bitrate khớp ±10% với bitrate nguồn (`ffmpeg -b:v 2M`).
- Xong khi: test xanh.

#### STR-05 Tự kết nối lại
- [ ] trạng thái
- Phụ thuộc: STR-04
- Làm: lỗi thì dựng lại pipeline sau 1, 2, 4, 8, ... tối đa 30 s; chạy ổn định 60 s thì đặt lại backoff; tăng
  `retry_count`. **Watchdog**: không có frame mới trong 10 s thì cũng coi là lỗi (có trường hợp mất hình mà không có
  `ERROR`).
- Kiểm thử:
  - (T) Tắt mediamtx 20 s rồi bật lại → luồng tự chạy lại, `retry_count > 0`.
  - (T) `docker pause eyerus-mtx` (treo, không ngắt kết nối) → watchdog dựng lại sau khoảng 10 s; `docker unpause` →
    chạy lại.
  - (T) Log thấy khoảng thời gian chờ tăng dần, không vượt 30 s.
- Xong khi: test xanh.

#### STR-06 StreamManager
- [ ] trạng thái
- Phụ thuộc: STR-05, STR-02
- Làm: quản lý nhiều luồng; lúc khởi động chạy lại mọi luồng `enabled`; thêm/sửa/xoá luồng lúc đang chạy; một luồng
  lỗi không ảnh hưởng luồng khác.
- Kiểm thử:
  - (T) 3 luồng (`cam01`, `cam02`, `cam03`); dừng nguồn `cam02` → chỉ `cam02` lỗi, 2 luồng còn lại vẫn tăng `frames`.
  - (T) Restart service → 3 luồng tự chạy lại.
- Xong khi: test xanh.

#### STR-07 Lấy nguồn từ camera-service
- [ ] trạng thái
- Phụ thuộc: STR-06, CAM-09, C-11
- Làm: luồng có `camera_id` thì gọi `camera.stream_uri` qua IPC để lấy URL (có mật khẩu) lúc dựng pipeline; không
  lưu mật khẩu trong `streams.json`.
- Kiểm thử:
  - (T) Tắt `eyerus-camera` → luồng báo `last_error: "camera-service unavailable"` và tự thử lại; bật lại → chạy.
  - (T) Đổi mật khẩu camera qua `camera.update` → `stream.restart` dùng mật khẩu mới.
- Xong khi: test xanh.

#### STR-08 Lệnh IPC và file trạng thái
- [ ] trạng thái
- Phụ thuộc: STR-06, C-10
- Làm: các lệnh `stream.*`; mỗi 2 s ghi `status/streams.json`
  `{updated_at, streams:[{id, state, frames, bytes, bitrate_kbps, retry_count, last_error, started_at}]}`.
- Kiểm thử:
  - (T) Script CRUD tương tự CAM-03.
  - (T) `watch -n1 jq . /run/eyerus/status/streams.json` thấy số liệu đổi liên tục.
  - (U) Dùng StatusReader (C-14): dừng service → sau 15 s file bị coi là `stale`.
- Xong khi: test xanh.

#### STR-09 Âm thanh
- [ ] trạng thái
- Phụ thuộc: STR-03
- Làm: `audio=auto`: có pad audio AAC/PCMA/PCMU thì chuyển tiếp kèm; audio lỗi thì dựng lại pipeline chỉ có hình.
- Kiểm thử:
  - (T) `cam03` có tiếng → `ffprobe` đầu ra có 2 stream.
  - (T) `cam01` không có tiếng → vẫn chạy.
  - (T) Nguồn có codec audio lạ (Opus) → fallback chỉ hình.
- Xong khi: test xanh.
- Tham khảo V2: `Tools/tests/test_stream_audio_fallback.py`.

#### STR-10 Webcam USB và zoom số
- [ ] trạng thái
- Phụ thuộc: CAM-12, STR-06
- Làm: pipeline `v4l2src ! (jpegdec nếu MJPEG) ! videoconvert ! videocrop ! videoscale ! x264enc tune=zerolatency
  speed-preset=ultrafast ! h264parse ! rtspclientsink`. Trên RK3566 thử bộ mã hoá phần cứng (`mpph264enc` /
  `v4l2h264enc`). `stream.zoom {level, x, y}` đổi thuộc tính `videocrop` lúc đang chạy.
- Kiểm thử:
  - (T) Dùng v4l2loopback (CAM-12) → `ffprobe` đầu ra là H.264.
  - (T) Zoom 2x → hình đầu ra phóng to vào giữa, kích thước giữ nguyên.
- Xong khi: test xanh.

#### STR-11 Nhánh khung hình cho AI
- [ ] trạng thái
- Phụ thuộc: STR-03
- Làm: khi `ai.enabled`, chèn `tee` sau parser:
  - nhánh 1: `queue ! rtspclientsink` (không đổi);
  - nhánh 2: `queue leaky=downstream max-size-buffers=2 ! avdec_h264 ! videorate ! video/x-raw,framerate=2/1 ! videoconvert ! video/x-raw,format=RGB ! shmsink socket-path=/run/eyerus/ai-<id>.shm wait-for-connection=false`.

  Nhánh AI chậm thì chỉ bỏ khung, **không được làm chậm** nhánh 1.
- Kiểm thử:
  - (T) `gst-launch-1.0 shmsrc socket-path=/run/eyerus/ai-cam01.shm is-live=true ! video/x-raw,format=RGB,width=1280,height=720,framerate=2/1 ! fakesink -v` thấy khung đến khoảng 2 lần/s.
  - (T) Treo bên đọc (`kill -STOP`) → nhánh 1 vẫn đủ fps.
  - (T) Ghi lại CPU khi bật/tắt AI.
- Xong khi: test xanh.

---

### SYS: system-service

`SystemService/`, chương trình `eyerus-system`, REST cổng 8080, socket `system.sock`.

Lệnh IPC: `system.info`, `system.status`, `system.time`, `system.time.set`, `system.reboot`, `network.status`,
`network.config`, `wifi.scan`, `wifi.connect`, `logs`, `services.status`, `service.restart`, `router.*`.

#### SYS-01 HTTP server trên Asio
- [ ] trạng thái
- Làm: lắng nghe TCP; đọc header tới `\r\n\r\n` (tối đa 16 KiB), body theo `Content-Length` (tối đa 1 MiB);
  timeout đọc 10 s; tách method, path, query, header. Mỗi kết nối xử lý trên thread pool (không xử lý lần lượt như V2,
  để một request chậm không chặn các request khác). Response luôn có `Content-Length` và `Connection: close`.
- Kiểm thử:
  - (T) `curl -i localhost:8080/api/health` → `200` + JSON.
  - (T) Body 2 MiB → `413`; request sai cú pháp (`printf 'xx\r\n\r\n' | nc localhost 8080`) → `400`.
  - (T) `nc localhost 8080` rồi không gửi gì → bị đóng sau 10 s; trong lúc đó `curl` khác vẫn trả lời ngay.
  - (T) `seq 200 | xargs -P 20 -I{} curl -s -o /dev/null -w '%{http_code}\n' localhost:8080/api/health | sort | uniq -c`
    → 200 lần `200`.
- Xong khi: test xanh.
- Tham khảo V2: `App/Api/HttpServer.cpp`.

#### SYS-02 Router đường dẫn
- [ ] trạng thái
- Làm: đăng ký `router.add("GET", "/api/cameras/{id}", handler)`; lấy tham số từ đường dẫn; sai method → `405`;
  không có route → `404`; parse query (`?lines=100`) có URL-decode.
- Kiểm thử: (U) bảng test: đường dẫn → handler + tham số; `/api/cameras/` (thiếu id) → 404; `%20` được giải mã.
- Xong khi: test xanh.

#### SYS-03 Xác thực
- [ ] trạng thái
- Phụ thuộc: C-03
- Làm:
  - `auth.json` lưu `{username, salt, iterations, hash}` bằng PBKDF2-HMAC-SHA256 (OpenSSL), so sánh bằng
    `CRYPTO_memcmp`. Mặc định `admin`/`admin`, bắt đổi lần đầu (cờ `must_change`).
  - HTTP Basic; sai thì `401` + `WWW-Authenticate: Basic realm="Eyerus"`.
  - Sai 5 lần từ cùng IP thì khoá IP đó 60 s, lần sau gấp đôi, tối đa 15 phút.
  - `PUT /api/auth/password`.
- Kiểm thử:
  - (T) Không auth → 401; sai → 401; đúng → 200.
  - (T) `grep admin /etc/eyerus/auth.json` không thấy mật khẩu thô.
  - (T) Sai 5 lần → lần 6 dù đúng vẫn `429` trong 60 s.
  - (U) Hash cùng mật khẩu với 2 salt khác nhau → 2 hash khác nhau.
- Xong khi: test xanh.
- Tham khảo V2: `App/Api/AuthStore.cpp`, `StartUp/Classic/ClassicAuth.cpp`.

#### SYS-04 Chuyển tiếp REST sang IPC
- [ ] trạng thái
- Phụ thuộc: SYS-02, C-11, CAM-03, STR-08
- Làm: bảng ánh xạ REST → (socket, cmd) như mục 3; body JSON → `args`; tham số đường dẫn gộp vào `args`; mã lỗi →
  HTTP status.
- Kiểm thử: (T) `SystemService/Scripts/test_rest.sh`:
  1. `curl -u admin:... POST /api/cameras` → 201
  2. `GET /api/cameras` → có camera
  3. `POST /api/streams` → luồng chạy
  4. `systemctl stop eyerus-camera` (hoặc kill) → `GET /api/cameras` → **503**
  5. Lệnh treo → **504** sau timeout
- Xong khi: script PASS.

#### SYS-05 Web UI
- [ ] trạng thái
- Phụ thuộc: SYS-04
- Làm: phục vụ file tĩnh `WebUi/` ở `/` (chống `../`: chuẩn hoá đường dẫn, chỉ cho phép đọc trong thư mục gốc; đúng
  `Content-Type`). Có thể chép UI từ V2 (`eyerus/WebUi/static`) rồi sửa dần cho API mới.
- Kiểm thử:
  - (T) Trình duyệt mở `http://<ip>:8080/` thấy UI.
  - (T) `curl --path-as-is localhost:8080/../../etc/passwd` → 404.
- Xong khi: thêm camera và luồng được từ UI.

#### SYS-06 Thông tin hệ thống
- [ ] trạng thái
- Làm: `system.info` lấy model (`/proc/device-tree/model`), OS (`/etc/os-release`), kernel (`uname`), `machine-id`,
  phiên bản. `system.status` lấy:
  - CPU % (đọc `/proc/stat` 2 lần, tính delta);
  - RAM (`/proc/meminfo` `MemAvailable`), đĩa (`statvfs`), uptime;
  - nhiệt độ (`/sys/class/thermal/thermal_zone*/temp`);
  - IP/MAC (`getifaddrs`).

  Phần parse viết thành hàm thuần.
- Kiểm thử:
  - (U) Parse các file `/proc` mẫu lưu trong fixtures.
  - (T) So với `free -m`, `df -h /`, `ip -br addr`, `uptime`: sai lệch nhỏ.
- Xong khi: test xanh.
- Tham khảo V2: `App/System/SystemInfo.cpp`.

#### SYS-07 Điều khiển hệ thống
- [ ] trạng thái
- Phụ thuộc: C-05, SYS-01
- Làm: `system.reboot` (trả lời trước, 3 s sau mới `systemctl reboot`; trong HTTP dùng cơ chế `afterSend`); múi giờ
  (`timedatectl set-timezone`); NTP (ghi drop-in `/etc/systemd/timesyncd.conf.d/eyerus.conf`); `services.status`,
  `service.restart` (chỉ cho các unit `eyerus-*`); `logs` (`journalctl -u <unit> -n N -o json`).
- Kiểm thử:
  - (T) Trong VM: reboot → curl nhận 200 trước khi máy tắt.
  - (T) `service.restart` với `ssh` → `bad_request`.
  - (T) `logs` 50 dòng → JSON đủ 50 phần tử.
  - Có cờ `--dry-run` (chỉ in lệnh) để test trên máy dev.
- Xong khi: test xanh.

#### SYS-08 Mạng và WiFi (nmcli)
- [ ] trạng thái
- Phụ thuộc: C-05
- Làm:
  - `network.status` (`nmcli -t -f ...`; viết parser xử lý escape `\:` và `\\` thành hàm thuần);
  - `network.config` (DHCP / IP tĩnh: 1 lệnh `nmcli connection modify` cho cả bộ, rồi `reapply`);
  - `wifi.scan`;
  - `wifi.connect`: khoá `flock`, lưu profile cũ, thử mạng mới, sau 30 s không có IP thì quay về mạng cũ.
- Kiểm thử:
  - (U) Parser với chuỗi `SSID\:la:80:WPA2` → SSID `SSID:la`.
  - (T) Trên board: kết nối WiFi đúng → có IP; sai mật khẩu → tự quay lại mạng cũ, vẫn SSH được.
  - (T) 2 lệnh `wifi.connect` cùng lúc → lệnh sau nhận `busy`.
- Xong khi: test trên board đạt.
- Tham khảo V2: `App/System/SystemControl.cpp`, `StartUp/Wifi/WifiManager.cpp`, `App/Api/WifiConnectJob.cpp`.

#### SYS-09 mDNS
- [ ] trạng thái
- Làm: ghi `/etc/avahi/services/eyerus.service` (`_http._tcp`, cổng 8080, TXT `version`, `id`); không có avahi thì
  bỏ qua.
- Kiểm thử: (T) Máy khác trong LAN: `avahi-browse -rt _http._tcp` thấy `Eyerus-XXXX`.
- Xong khi: test xanh.
- Tham khảo V2: `App/Api/MdnsAnnouncer.cpp`.

#### SYS-10 Router: planner thuần
- [ ] trạng thái
- Làm: `RouterConfig` (cổng LAN, IP/mask, DHCP range, đặt trước IP theo MAC, mode `isolated`/`router`, firewall
  allowlist) → các hàm thuần `buildDnsmasqConfig()`, `buildNftScript()`. Bỏ các phần tử interval bị chứa trong phần
  tử khác (nft báo "conflicting intervals").
- Kiểm thử:
  - (U) Golden file: config mẫu → so với `Test/golden/*.conf`, `*.nft`.
  - (U) Allowlist có `10.0.0.0/8` và `10.1.2.3` → chỉ còn `10.0.0.0/8`.
  - (T) `nft -c -f out.nft` (chỉ kiểm cú pháp, không nạp) → mã 0.
- Xong khi: test xanh.
- Tham khảo V2: `StartUp/Router/RouterPlanner.*`.

#### SYS-11 Router: áp dụng, xác nhận, quay lại
- [ ] trạng thái
- Phụ thuộc: SYS-10
- Làm: nạp `nft -f` (một giao dịch); viết config dnsmasq và chạy dnsmasq; áp dụng xong chờ người dùng gửi
  `router.confirm` trong 120 s, không thì tự rollback. Phân giải domain trong allowlist mỗi 30 s (giữ IP cũ 30 phút).
  Domain Commadesk luôn được phép.
- Kiểm thử:
  - (T) Network namespace (tham khảo `eyerus/Tools/tests/test_firewall_e2e.py`): ns LAN lấy được DHCP; mode
    `isolated` → ns LAN không ping được WAN; mode `router` → ping được.
  - (T) Không gửi confirm → sau 120 s rule cũ quay lại.
  - (T) Firewall bật: `curl` tới domain trong list được, ngoài list bị chặn.
- Xong khi: e2e PASS.

---

### CLD: cloud-service

`CloudService/`, chương trình `eyerus-cloud`, socket `cloud.sock`.

Lệnh IPC: `cloud.status`, `cloud.publish_now`, `cloud.link.start`, `cloud.link.status`, `server.domain.get/set/rollback`.

#### CLD-01 MQTT client
- [ ] trạng thái
- Làm: bọc libmosquitto:
  - `mosquitto_loop_start` (thread riêng của thư viện), tự kết nối lại (`reconnect_delay_set`).
  - Last Will `offline`, gửi `online` (retain) khi kết nối; subscribe lại trong `on_connect`.
  - Publish QoS 1; có hàng đợi khi mất kết nối (giới hạn 1000 bản tin, bỏ bản cũ nhất).
  - TLS tuỳ chọn.
- Kiểm thử (mosquitto local):
  - (T) `mosquitto_sub -v -t '#'` thấy `online`.
  - (T) `kill -9` service → thấy `offline` (Last Will).
  - (T) Restart broker → service tự kết nối lại và subscribe lại (gửi thử 1 lệnh thì nhận được).
  - (T) Tắt broker 1 phút trong lúc có telemetry → bật lại thì các bản tin bị giữ được gửi đi.
- Xong khi: test xanh.
- Tham khảo V2: `Server/Mqtt/MqttPublisher.cpp`.

#### CLD-02 Cấu hình và nạp lại
- [ ] trạng thái
- Phụ thuộc: CLD-01, C-13
- Làm: `cloud.json` (broker, port, tls, username, password, topic prefix, chu kỳ); file đổi → kết nối lại.
- Kiểm thử: (T) Sửa `port` sang broker thứ 2 (`mosquitto -p 1884`) → service chuyển sang broker đó trong dưới 3 s.
- Xong khi: test xanh.

#### CLD-03 Thu thập telemetry
- [ ] trạng thái
- Phụ thuộc: C-14, SYS-06 (chuyển phần parse `/proc` vào `Common` để dùng chung)
- Làm: số liệu máy (cpu, mem, disk, temp); đọc `status/streams.json`, `status/sensors.json`, `status/ai-events.json`;
  file `stale` thì gửi kèm cờ `stale` thay vì số cũ.
- Kiểm thử:
  - (U) Collector với thư mục fixture → JSON đúng.
  - (T) Dừng streaming-service → sau 15 s bản tin có `streams.stale=true`.
- Xong khi: test xanh.

#### CLD-04 Gửi telemetry theo chu kỳ
- [ ] trạng thái
- Phụ thuộc: CLD-01, CLD-03, C-09
- Làm: topic `commadesk/<org>/<dev>/telemetry` (60 s), `.../status` (30 s, chi tiết luồng), `.../events` (`boot`,
  `device_info` mỗi lần kết nối). Mỗi bản tin có `message_id` (UUID), `ts` (UTC). Lệnh `cloud.publish_now`. Ghi
  `status/cloud.json` mỗi 1 s.
- Kiểm thử:
  - (T) `mosquitto_sub -t 'commadesk/#' | jq .` kiểm định dạng từng loại.
  - (T) Đo khoảng cách giữa 2 bản tin = 60 s ±1 s.
  - (T) `cloud.publish_now` → có bản tin ngay.
  - So định dạng với tài liệu server: `eyerus/Docs/API.md` mục "Gửi telemetry qua MQTT".
- Xong khi: test xanh.

#### CLD-05 Lệnh từ xa qua MQTT
- [ ] trạng thái
- Phụ thuộc: CLD-01, C-11
- Làm: subscribe `.../commands`; tra bảng `prefix → socket` (`camera.*` → camera.sock, `stream.*` → streaming.sock,
  `system.*`, `network.*`, `wifi.*`, `router.*` → system.sock, ...); gọi IPC với `source:"mqtt"`; trả kết quả lên
  `.../command-ack` (giữ `command_id`). Lệnh trùng `command_id` trong 10 phút thì không chạy lại, chỉ gửi lại kết quả
  cũ (QoS 1 có thể giao 2 lần).
- Kiểm thử:
  - (T) `mosquitto_pub -t .../commands -m '{"command_id":"c1","command":"camera.list"}'` → ack có danh sách.
  - (T) Gửi lại `c1` → ack giống hệt, camera-service không nhận lệnh lần 2 (xem log).
  - (T) Lệnh lạ → ack `unknown_command`; service đích tắt → `unavailable`.
- Xong khi: test xanh.
- Tham khảo V2: `Server/Control/MqttControlService.cpp`, `Docs/GATEWAY_CONTROL.md`.

#### CLD-06 Định danh thiết bị và ký HTTP
- [ ] trạng thái
- Phụ thuộc: C-07, C-09
- Làm:
  - `identity.json` (600): `device_id`, `signing_secret`, `api_base_url`.
  - `SignedHttpClient` (libcurl): thêm header timestamp, nonce, chữ ký HMAC-SHA256 theo chuỗi chuẩn của server.
    Đọc chính xác cách ghép chuỗi trong `eyerus/Server/DeviceLink/DeviceLinkClient.cpp` và tài liệu
    `device-identity-authentication.md`.
  - Viết `Tools/fake_commadesk.py`: server HTTP Python kiểm chữ ký bằng `hmac`, từ chối khi lệch giờ quá 5 phút hoặc
    nonce bị dùng lại.
- Kiểm thử:
  - (U) Chuỗi ký và chữ ký khớp với kết quả tính bằng Python cho cùng đầu vào cố định.
  - (T) `fake_commadesk.py` chấp nhận request; sửa 1 byte body → 401; gửi lại cùng nonce → 401.
- Xong khi: test xanh.

#### CLD-07 Enroll / đăng ký thiết bị
- [ ] trạng thái
- Phụ thuộc: CLD-06
- Làm: 2 cách:
  - `--enroll <pair_code>`;
  - `cloud.link.start`: register mode open → trả `claim_url`/QR → poll `registration-status` → `active` thì gọi
    `bootstrap` → lưu `identity.json`.

  Sau đó `GET /device-link/mqtt` lấy tài khoản broker và ghi vào `cloud.json`.
- Kiểm thử:
  - (T) Với `fake_commadesk.py`: chạy hết luồng, có `identity.json` quyền 600, MQTT tự kết nối bằng tài khoản mới.
  - (T) Ngắt mạng giữa lúc poll → poll tiếp được khi có mạng lại.
  - (T) Cuối cùng thử với server dev thật.
- Xong khi: liên kết được với server dev.
- Tham khảo V2: `Server/DeviceLink/DeviceRegistration.cpp`, `Docs/LIEN-KET-THIET-BI.md`.

#### CLD-08 Heartbeat và lệnh vận hành
- [ ] trạng thái
- Phụ thuộc: CLD-07
- Làm: `PUT /device-identity/heartbeat` mỗi 60 s (phiên bản, IP LAN); `GET /device-commands/check` mỗi 10 s →
  chạy qua cùng đường với CLD-05 → `POST /device-commands/report`. Broker từ chối (CONNACK 4/5) → lấy lại tài khoản
  MQTT; ngoài ra lấy lại mỗi 6 giờ.
- Kiểm thử:
  - (T) Log của `fake_commadesk.py` có heartbeat đều đặn.
  - (T) Đặt lệnh vào fake server → được thực thi và report.
  - (T) Đổi mật khẩu broker → service tự lấy tài khoản mới và kết nối lại.
- Xong khi: test xanh.

#### CLD-09 Đồng bộ giờ qua MQTT
- [ ] trạng thái
- Phụ thuộc: CLD-01
- Làm: gửi `eyerus/time/<gw>/req`, nhận `.../set`; bù nửa thời gian khứ hồi (RTT/2); lệch quá 5 s thì đặt giờ
  (`clock_settime` hoặc `timedatectl set-time`). Phần tính toán viết thành hàm thuần.
- Kiểm thử:
  - (U) Lệch 3 s → không chỉnh; lệch 10 s → chỉnh; tính có bù RTT/2.
  - (T) Script Python giả trả giờ lệch 1 phút; chạy với `--dry-run` → log báo sẽ chỉnh đúng 60 s.
- Xong khi: test xanh.
- Tham khảo V2: `Server/Telemetry/ClockSync.cpp`.

#### CLD-10 OTA phần 1: nhận offer
- [ ] trạng thái
- Phụ thuộc: CLD-01
- Làm: máy trạng thái `idle → offered → downloading → downloaded → installing → done/failed`.
  - `ota_query` → `ota_status`.
  - `ota_offer` → kiểm tra phiên bản mới hơn, đủ chỗ đĩa (gấp 2 lần kích thước gói) → `ota_ready`, hoặc từ chối có
    lý do.
  - Lưu `state.json` mỗi lần đổi trạng thái.

  Viết `Tools/fake_ota_server.py` (paho-mqtt) đóng vai server.
- Kiểm thử:
  - (U) Bảng chuyển trạng thái: sự kiện sai trạng thái thì bị bỏ qua và có log.
  - (T) Offer phiên bản cũ hơn → bị từ chối; đĩa không đủ (giả bằng kích thước lớn) → bị từ chối.
- Xong khi: test xanh.
- Tham khảo V2: `Server/Ota/OtaAgent.cpp`, `Docs/API.md` mục OTA.

#### CLD-11 OTA phần 2: nhận chunk
- [ ] trạng thái
- Phụ thuộc: CLD-10, C-06, C-08
- Làm: chunk base64 → decode → kiểm SHA-256 của chunk → `pwrite` đúng offset → `fsync` → **ghi xong mới** `ota_ack`.
  Chunk hỏng thì `ota_retry`/`ota_nack`. Bitmap chunk đã nhận nằm trong `state.json`. Đủ chunk thì kiểm SHA-256 cả
  file → `ota_downloaded`.
- Kiểm thử:
  - (T) Fake server gửi 100 chunk → file ghép khớp `sha256sum`.
  - (T) Làm hỏng chunk 37 → nhận retry, gửi lại đúng → hoàn tất.
  - (T) `kill -9` service ở chunk 50 rồi chạy lại → `ota_status` báo đã có 50 chunk, tải tiếp từ 51.
  - (T) Gửi chunk ngoài thứ tự / trùng → kết quả vẫn đúng.
- Xong khi: test xanh.

#### CLD-12 OTA phần 3: cài và quay lại bản cũ
- [ ] trạng thái
- Phụ thuộc: CLD-11, PKG-02
- Làm:
  - Kiểm gói (`dpkg-deb -f pkg.deb Package` phải là `eyerus`).
  - Lưu bản đang cài vào `current.deb`.
  - Chạy `systemd-run --unit=eyerus-ota-apply --collect sh -c 'dpkg -i ...; echo $? > apply.rc'`, để tiến trình cài
    vẫn sống khi chính `eyerus-cloud` bị restart lúc nâng cấp.
  - Sau khi chạy lại, đọc `state.json` + `apply.rc` → kiểm các service `active` → `done`; lỗi thì cài lại
    `current.deb` → `failed + rolled_back`. Có `--ota-dry-run`.
- Kiểm thử (VM hoặc board, cần systemd thật):
  - (T) Gói tốt bản +1 → `done`, `dpkg -l eyerus` ra phiên bản mới.
  - (T) Gói có `postinst` `exit 1` → `rolled_back`, phiên bản cũ vẫn chạy.
  - (T) Rút điện (hoặc `systemctl kill`) đúng lúc `installing` → khởi động lại thì tự sửa (`dpkg --configure -a`) và
    báo kết quả.
- Xong khi: 3 kịch bản đạt.
- Tham khảo V2: `Server/Ota/OtaSystem.cpp`, `Server/OtaPackage/README.md`.

#### CLD-13 Đổi domain server (làm sau)
- [ ] trạng thái
- Phụ thuộc: CLD-08
- Làm: `server.domain.set`: sao lưu → ack qua kết nối cũ → áp dụng domain mới → trong 300 s cần có CONNACK + phản
  hồi giờ + heartbeat thì chốt; không thì tự quay về. Restart giữa chừng cũng quay về.
- Kiểm thử: (T) 2 broker local: đổi sang broker đúng → `committed`; đổi sang broker sai → sau thời gian thử thì quay
  lại broker cũ.
- Tham khảo V2: `Server/Control/ServerDomain.cpp`.

---

### DEV: device-service

`DeviceService/`, chương trình `eyerus-device`, socket `device.sock`. Event loop: **sd-event** (cần cho sd-bus).

Lệnh IPC: `device.status`, `device.led.identify`, `device.sensors`, `device.ble.status`, `device.ble.forget`,
`device.factory_reset`, `device.factory_undo`, `device.pairing.open`.

#### DEV-01 Event loop sd-event + IPC
- [ ] trạng thái
- Phụ thuộc: C-10
- Làm: chạy IPC server trên sd-event (thêm fd của socket bằng `sd_event_add_io`), hoặc chạy IPC ở thread riêng rồi
  chuyển việc vào loop bằng `eventfd`. Timer dùng `sd_event_add_time`.
- Kiểm thử: (T) `ping` qua socat; `kill -TERM` thoát sạch.
- Tham khảo V2: `StartUp/Event/`, `StartUp/Provision/ProvisionApplication.cpp`.

#### DEV-02 GPIO chardev v2
- [ ] trạng thái
- Làm: `GpioLine` dùng `GPIO_V2_GET_LINE_IOCTL`: output, input có pull-up, active-low, sự kiện cạnh lên/xuống kèm
  timestamp (`GPIO_V2_LINE_FLAG_EDGE_*`, đọc `gpio_v2_line_event`), debounce. Chọn chân theo tên (`PIN_11`) qua bảng
  ánh xạ theo board.
- Kiểm thử:
  - (T) `sudo modprobe gpio-mockup gpio_mockup_ranges=-1,8` → set/get trên chip giả; `gpioinfo` thấy consumer
    `eyerus`.
  - (T) Board thật: LED sáng/tắt, đọc nút.
  - (T) Chân đang bị chiếm → lỗi `busy` rõ ràng.
- Tham khảo V2: `StartUp/Gpio/GpioLine.cpp`.

#### DEV-03 LED trạng thái
- [ ] trạng thái
- Phụ thuộc: DEV-02
- Làm: các kiểu `on`, `off`, `blink_slow`, `blink_fast`, `double_blink`, `identify(30 s)`; một lớp chọn kiểu theo
  trạng thái tổng (có mạng, setup mode, đang kết nối WiFi, đang giữ nút).
- Kiểm thử:
  - (U) Bảng trạng thái → kiểu nháy.
  - (T) `device.led.identify` → LED trên board nháy 30 s rồi trở lại kiểu cũ.

#### DEV-04 Logic nút bấm (thuần)
- [ ] trạng thái
- Làm: máy trạng thái nhận chuỗi `(press|release, t_ms)` → sự kiện `click`, `hold_progress`,
  `factory_reset (giữ 10–30 s)`, `cancel (giữ > 30 s)`, `triple_click (trong 5 phút sau reset)`.
- Kiểm thử (U), dùng dòng thời gian giả:
  - Bấm 200 ms → `click`; giữ 12 s → `factory_reset`; giữ 35 s → không reset.
  - 3 lần bấm trong 1,5 s sau reset → `undo`; 2 lần → không.
  - Nhiễu 5 ms (dội phím) bị bỏ qua.
- Tham khảo V2: `StartUp/Gpio/ButtonLogic.cpp`.

#### DEV-05 Cảm biến DHT11
- [ ] trạng thái
- Phụ thuộc: DEV-02
- Làm: kéo chân xuống 18 ms → chuyển sang input → đo độ dài xung bằng `clock_gettime(CLOCK_MONOTONIC)` (hoặc
  timestamp của sự kiện cạnh) → 40 bit (bit 1 khi mức cao dài hơn khoảng 50 µs) → checksum. Phần giải mã độ dài xung
  → giá trị viết thành hàm thuần. Đọc lỗi thì thử lại, tối đa 1 lần mỗi 2 s.
- Kiểm thử:
  - (U) Mảng độ dài xung ghi sẵn → 26 °C / 60 %; checksum sai → lỗi.
  - (T) Board thật: so với nhiệt kế; 100 lần đọc ≥ 90% thành công.
- Tham khảo V2: `Sensor/Dht11/Dht11Sensor.cpp`.

#### DEV-06 I2C mềm + AGS10
- [ ] trạng thái
- Phụ thuộc: DEV-02
- Làm: `SoftI2cBus` (start/stop/ack, open-drain giả bằng cách chuyển input/output); AGS10: đọc TVOC, kiểm CRC-8
  theo datasheet, đổi TVOC sang AQI.
- Kiểm thử:
  - (U) CRC-8 với vector trong datasheet AGS10.
  - (U) Bảng TVOC → AQI tại các mốc biên.
  - (T) Board thật: đọc ổn định; rút dây → lỗi `no_ack`, service vẫn chạy.
- Tham khảo V2: `Sensor/Gpio/SoftI2cBus.cpp`, `Sensor/Ags10/`.

#### DEV-07 File trạng thái cảm biến
- [ ] trạng thái
- Phụ thuộc: DEV-05, DEV-06
- Làm: ghi `status/sensors.json` mỗi 2 s; lệnh `device.sensors`.
- Kiểm thử: (T) `jq` thấy số liệu; CLD-03 đọc được.

#### DEV-08 sd-bus + BlueZ cơ bản
- [ ] trạng thái
- Phụ thuộc: DEV-01
- Làm: kết nối system bus; đọc thuộc tính `org.bluez.Adapter1` (Address, Powered, Name); bật Powered; đặt Alias
  `Eyerus-XXXX`; theo dõi `PropertiesChanged` và `InterfacesAdded` (thiết bị kết nối / ngắt).
- Kiểm thử:
  - (T) So kết quả với `busctl introspect org.bluez /org/bluez/hci0` và `bluetoothctl show`.
  - (T) `systemctl restart bluetooth` → service tự kết nối lại với BlueZ.
- Tham khảo V2: `StartUp/Ble/DbusConnection.cpp`, `BluezAdapter.cpp`.

#### DEV-09 Quảng bá BLE + GATT server tối thiểu
- [ ] trạng thái
- Phụ thuộc: DEV-08
- Làm: export object `LEAdvertisement1` và GATT `Service1`/`Characteristic1` bằng `sd_bus_add_object_vtable`; có
  `ObjectManager`; `RegisterAdvertisement`, `RegisterApplication`. Một characteristic đọc/ghi thử.
- Kiểm thử:
  - (T) App nRF Connect thấy `Eyerus-XXXX`, đọc được giá trị, ghi được giá trị.
  - (T) `btmon` thấy gói quảng bá đúng UUID.
- Tham khảo V2: `StartUp/Ble/Advertiser.cpp`, `GattServer.cpp`.

#### DEV-10 Improv WiFi
- [ ] trạng thái
- Phụ thuộc: DEV-09, SYS-08
- Làm: codec Improv (thuần: frame, checksum, RPC `WIFI_SETTINGS`); service GATT theo UUID của chuẩn Improv; nhận
  SSID/mật khẩu → gọi `wifi.connect` của system-service qua IPC → trả URL `http://<ip>:8080`. Chỉ nhận khi chưa có
  mạng, đang ở setup mode, hoặc cửa sổ ghép đang mở.
- Kiểm thử:
  - (U) Frame mẫu theo đặc tả Improv: encode/decode, checksum sai bị từ chối.
  - (T) Điện thoại mở `https://www.improv-wifi.com/` → cài WiFi → nhận URL.
  - (T) Máy đã có mạng và cửa sổ ghép đóng → bị từ chối (`not authorized`).
- Tham khảo V2: `StartUp/Improv/ImprovCodec.cpp`, `StartUp/Provision/ImprovService.cpp`.

#### DEV-11 Factory reset và hoàn tác
- [ ] trạng thái
- Phụ thuộc: DEV-04
- Làm: sao lưu `/etc/eyerus/*.json` + profile mạng (`nmcli` export) → xoá camera / luồng / WiFi → quên thiết bị
  Bluetooth → tạo cờ `setup-mode` → restart các service. Giữ lại `identity.json` và `auth.json`. Hoàn tác: chép lại
  bản sao lưu. Mọi đường dẫn đều cấu hình được để test trên thư mục tạm.
- Kiểm thử:
  - (U) Thư mục giả: reset → đúng file bị xoá / còn giữ; undo → giống hệt trước (so `sha256sum`).
  - (T) Board thật: giữ nút 12 s → setup mode; bấm 3 lần → trở lại như cũ.

#### DEV-12 Bluetooth Classic SPP + đăng nhập
- [ ] trạng thái
- Phụ thuộc: DEV-08, C-11, SYS-03
- Làm: export `org.bluez.Profile1` (UUID SPP, chọn kênh RFCOMM khác 3); `NewConnection` trả về fd → đọc từng dòng
  JSON; agent ghép (`Agent1`) chỉ cho ghép khi cửa sổ ghép đang mở; lệnh `login` dùng tài khoản REST; sai 5 lần thì
  khoá tăng dần; sau khi đăng nhập thì chuyển lệnh qua IPC như CLD-05.
- Kiểm thử:
  - (T) App Android "Serial Bluetooth Terminal" ghép trong cửa sổ ghép → `login` → `system.status` ra JSON.
  - (T) Ngoài cửa sổ ghép → không ghép được.
  - (T) Chưa login mà gửi lệnh → `unauthorized`.
- Tham khảo V2: `StartUp/Ble/SppProfile.cpp`, `PairingAgent.cpp`, `StartUp/Classic/`.

---

### AI: ai-service

`AIService/`, chương trình `eyerus-ai`, socket `ai.sock`.

#### AI-01 Nhận khung hình
- [ ] trạng thái
- Phụ thuộc: STR-11
- Làm: pipeline `shmsrc ! video/x-raw,... ! appsink`; callback nhận khung RGB; đo fps; mất nguồn thì thử lại.
- Kiểm thử:
  - (T) fps nhận ≈ 2.
  - (T) Restart streaming-service → AI tự nhận lại.
  - (T) Lưu 1 khung ra PNG để xem bằng mắt.

#### AI-02 Chọn và chạy model
- [ ] trạng thái
- Làm: chọn runtime: ONNX Runtime (CPU) để làm nhanh; trên RK3566 dùng **RKNN** (NPU 0,8 TOPS) để tiết kiệm CPU.
  Bắt đầu bằng phát hiện người / chuyển động. Phần tiền xử lý (resize, normalize) và hậu xử lý (NMS) viết thành hàm
  thuần.
- Kiểm thử:
  - (U) NMS với các hộp mẫu.
  - (T) Ảnh mẫu có người → phát hiện đúng; đo thời gian suy luận trên board.

#### AI-03 Sự kiện
- [ ] trạng thái
- Phụ thuộc: AI-02, CLD-04
- Làm: gộp phát hiện thành sự kiện (bắt đầu / kết thúc, chống báo liên tục); ghi `status/ai-events.json`;
  cloud-service gửi lên `.../events`.
- Kiểm thử: (T) Video mẫu có người đi qua → đúng 1 sự kiện `person_detected` lên MQTT.

---

### PKG: Đóng gói và triển khai

#### PKG-01 Unit systemd
- [ ] trạng thái
- Phụ thuộc: C-12
- Làm: mỗi service một file `eyerus-xxx.service`:
  - `Type=notify`, `Restart=on-failure`, `RestartSec=2`;
  - `RuntimeDirectory=eyerus`, `RuntimeDirectoryPreserve=yes` (các service dùng chung `/run/eyerus`);
  - `After=network-online.target`, riêng device-service thêm `After=bluetooth.service`;
  - target chung `eyerus.target`.
- Kiểm thử:
  - (T) `systemctl start eyerus.target` → cả 6 service `active`.
  - (T) `kill -9` một service → systemd chạy lại trong khoảng 2 s; service khác không bị ảnh hưởng.
  - (T) `systemd-analyze verify *.service` không có lỗi.

#### PKG-02 Gói .deb
- [ ] trạng thái
- Phụ thuộc: PKG-01
- Làm: `Packaging/build_deb.sh` dùng `dpkg-deb --build`. `postinst` chép config mẫu nếu chưa có, tạo nhóm `eyerus`,
  enable unit. **Không** đưa file trong `/etc/eyerus` vào gói (OTA sẽ xoá file thiếu trong gói mới).
- Kiểm thử:
  - (T) Container hoặc VM Debian: `dpkg -i` → các service chạy.
  - (T) Cài bản mới đè lên → config giữ nguyên.
  - (T) `apt purge` → sạch.
- Tham khảo V2: `Packaging/deb/`.

#### PKG-03 Build arm64
- [ ] trạng thái
- Làm: build trong Docker arm64 (`docker buildx` + qemu) hoặc build ngay trên board với `-j2`.
- Kiểm thử:
  - (T) `file build/.../eyerus-camera` → `ARM aarch64`.
  - (T) Cài gói lên board 192.168.1.193 (dừng V2 trước) → `eyerus.target` chạy.

---

## 8. Kiểm thử tích hợp toàn hệ thống

Làm sau khi xong CAM, STR, SYS, CLD (chưa cần DEV và AI). Viết thành `Tests/e2e/full_flow.sh`:

1. Bật mediamtx + `fake_onvif.py` + mosquitto + `fake_commadesk.py`.
2. Chạy 4 service (hoặc `systemctl start eyerus.target`).
3. REST: scan → thêm camera ONVIF → tạo luồng tới `rtsp://localhost:8554/out/cam01`.
4. `ffprobe` đầu ra có hình.
5. Trong 60 s, MQTT nhận telemetry có `stream_running=1`.
6. MQTT gửi `stream.stop` → ack ok → `ffprobe` đầu ra thất bại.
7. `kill -9 eyerus-streaming` → systemd chạy lại → luồng (nếu đang bật) tự chạy lại.
8. Tắt mediamtx 30 s → telemetry báo luồng `error` → bật lại → `running`.

**Xong khi:** script PASS 3 lần liên tiếp.

---

## 9. Nhật ký tiến độ

Ghi mỗi khi xong một task (mới nhất ở trên). Ghi cả lỗi khó và điều học được để sau tra lại.

| Ngày | Task | Kết quả kiểm thử | Ghi chú / điều học được |
|---|---|---|---|
| 2026-11-14 | (khởi tạo) | — | Tạo CodeBase.md; khung CameraService có sẵn (chỉ header) |
