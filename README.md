# Linux Embedded IoT Gateway

Dự án mô phỏng một IoT Edge Gateway: ESP32 gửi dữ liệu nhiệt độ qua TCP/IP, Linux gateway nhận nhiều kết nối bằng `epoll`, đồng bộ dữ liệu giữa các luồng bằng POSIX threads và lưu kết quả vào SQLite.

## Thành phần chính

```text
GateWay/
├── FirmwareESP32/          # Firmware ESP32 dùng Arduino framework qua PlatformIO
│   ├── platformio.ini
│   └── src/main.cpp
├── linux_server/           # TCP gateway chạy trên Linux
│   ├── Makefile
│   ├── main.c
│   ├── stress_test.py
│   └── gateway_data.db     # SQLite database sinh/ghi bởi gateway
└── README.md
```

## Kiến trúc hiện tại

```text
ESP32 / stress_test.py
        |
        | TCP payload: [TEMP:<value>]
        v
Linux gateway :8080
        |
        | producer thread
        | - socket/bind/listen/accept
        | - epoll để theo dõi nhiều client
        | - parse payload [TEMP:%d]
        v
shared state: sensor_data, data_ready
        |
        | pthread_mutex_t + pthread_cond_t
        v
consumer thread
        |
        | INSERT INTO SensorData(Temp)
        v
SQLite: linux_server/gateway_data.db
```

### Linux gateway

File chính: `linux_server/main.c`

- Lắng nghe TCP trên port `8080`.
- Dùng non-blocking socket và `epoll_wait()` để xử lý nhiều client.
- Chấp nhận payload dạng `[TEMP:28]`.
- Producer thread cập nhật dữ liệu mới nhất vào biến dùng chung.
- Consumer thread chờ tín hiệu bằng `pthread_cond_wait()` rồi ghi nhiệt độ vào SQLite.
- Database được mở tại thư mục chạy chương trình, mặc định là `linux_server/gateway_data.db`.
- Bảng dữ liệu:

```sql
CREATE TABLE IF NOT EXISTS SensorData(
    Id INTEGER PRIMARY KEY AUTOINCREMENT,
    Temp INT
);
```

Lưu ý: code hiện tại đang giới hạn consumer xử lý `MAX_ITEMS = 5` mẫu trong mỗi lần chạy. Sau khi đủ 5 mẫu, consumer đóng database; producer thread vẫn tiếp tục lắng nghe TCP.

### Firmware ESP32

File chính: `FirmwareESP32/src/main.cpp`

- Board PlatformIO hiện tại: `esp32cam`.
- Kết nối Wi-Fi bằng `WIFI_SSID` và `WIFI_PASSWORD`.
- Kết nối TCP đến `SERVER_IP:8080`.
- Mỗi giây sinh nhiệt độ giả trong khoảng `20..40`.
- Gửi payload dạng `[TEMP:<value>]`, ví dụ `[TEMP:31]`.
- Tự kết nối lại khi mất Wi-Fi hoặc TCP server chưa sẵn sàng.

## Yêu cầu môi trường

### Linux gateway

Cần compiler C, pthread và SQLite development library:

```bash
sudo apt update
sudo apt install build-essential libsqlite3-dev python3
```

Tùy chọn, nếu muốn đọc database bằng CLI:

```bash
sudo apt install sqlite3
```

### ESP32 firmware

Cần PlatformIO. Có thể dùng PlatformIO extension trong VS Code hoặc PlatformIO Core CLI.

## Build và chạy Linux gateway

```bash
cd linux_server
make
./gateway_app
```

Khi chạy thành công, gateway sẽ in log tương tự:

```text
=== BOOTING LINUX IOT GATEWAY ===
[TCP Server] Dang lang nghe tai Port 8080 bang EPOLL...
```

Nếu muốn build lại từ đầu:

```bash
cd linux_server
make clean
make
```

## Cấu hình và nạp firmware ESP32

Mở `FirmwareESP32/src/main.cpp` và chỉnh các hằng số cấu hình ở đầu file:

```cpp
const char* WIFI_SSID = "your-wifi-ssid";
const char* WIFI_PASSWORD = "your-wifi-password";
const char* SERVER_IP = "192.168.x.x";
const uint16_t SERVER_PORT = 8080;
```

`SERVER_IP` phải là IP của máy Linux đang chạy `gateway_app` trong cùng mạng với ESP32. Có thể kiểm tra IP trên Linux bằng:

```bash
ip addr
```

Build, upload và mở serial monitor:

```bash
cd FirmwareESP32
pio run -e esp32cam
pio run -e esp32cam -t upload
pio device monitor -b 115200
```

Nếu dùng VS Code PlatformIO, có thể chạy các tác vụ Build, Upload và Monitor từ giao diện PlatformIO.

## Stress test không cần ESP32

Script `linux_server/stress_test.py` mô phỏng nhiều ESP32 client kết nối tới `127.0.0.1:8080`.

Chạy gateway ở terminal thứ nhất:

```bash
cd linux_server
./gateway_app
```

Chạy stress test ở terminal thứ hai:

```bash
cd linux_server
python3 stress_test.py
```

Cấu hình mặc định trong script:

- `SERVER_IP = "127.0.0.1"`
- `SERVER_PORT = 8080`
- `NUM_CLIENTS = 200`
- Mỗi client gửi 5 payload, cách nhau 0.5 giây.

Do consumer hiện chỉ ghi tối đa 5 mẫu mỗi lần chạy, stress test chủ yếu dùng để kiểm tra khả năng nhận kết nối và xử lý sự kiện TCP bằng `epoll`.

## Kiểm tra dữ liệu SQLite

Nếu đã cài `sqlite3`, có thể xem dữ liệu đã lưu:

```bash
cd linux_server
sqlite3 gateway_data.db "SELECT * FROM SensorData ORDER BY Id DESC LIMIT 10;"
```

Hoặc xem schema:

```bash
sqlite3 gateway_data.db ".schema SensorData"
```

## Dọn file build

```bash
cd linux_server
make clean
```

## Ghi chú phát triển

- Payload TCP hiện chỉ hỗ trợ đúng format `[TEMP:%d]`.
- Producer đang lưu một giá trị nhiệt độ mới nhất thay vì hàng đợi nhiều phần tử.
- `listen(server_fd, 3)` đang dùng backlog nhỏ; nếu stress test nhiều client, có thể cần tăng backlog.
- Wi-Fi SSID, mật khẩu và IP server đang được cấu hình trực tiếp trong firmware. Khi public repo, nên chuyển thông tin nhạy cảm sang file cấu hình local hoặc biến build.
