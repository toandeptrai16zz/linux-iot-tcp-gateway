# Linux Embedded IoT TCP Gateway

## 📌 Overview
An industrial-grade Edge Gateway system that bridges hardware edge nodes (ESP32) with a Linux-based processing server over Wi-Fi (TCP/IP). This project demonstrates core concepts in **Linux System Programming**, specifically concurrent multi-threading and network socket programming.

Hệ thống Edge Gateway cấp công nghiệp, kết nối các node phần cứng biên (ESP32) với máy chủ xử lý chạy Linux thông qua Wi-Fi (TCP/IP). Dự án thể hiện các kiến thức cốt lõi về **Linux System Programming**, đặc biệt là lập trình đa luồng đồng thời và lập trình network socket.

##  System Architecture
The repository consists of two decoupled components:

Kho mã nguồn gồm hai thành phần độc lập:

1. **Linux TCP Server Daemon (`linux_server/`)**
   - Written in pure C targeting Linux User-Space.
   - Được viết bằng C thuần, hướng đến môi trường Linux User-Space.
   - **Multi-threading (POSIX Threads):** Utilizes `pthreads` to run a Producer-Consumer architecture.
   - **Đa luồng (POSIX Threads):** Sử dụng `pthreads` để vận hành kiến trúc Producer-Consumer.
   - **TCP Socket API:** The Producer thread acts as a blocking TCP server (`socket`, `bind`, `listen`, `accept`) to receive telemetry data over the LAN.
   - **TCP Socket API:** Producer thread hoạt động như một TCP server dạng blocking (`socket`, `bind`, `listen`, `accept`) để nhận dữ liệu telemetry qua mạng LAN.
   - **Concurrency Control:** Employs `pthread_mutex_t` to safeguard the shared data queue, preventing race conditions between network I/O and data processing threads.
   - **Kiểm soát đồng thời:** Sử dụng `pthread_mutex_t` để bảo vệ hàng đợi dữ liệu dùng chung, ngăn race condition giữa luồng network I/O và luồng xử lý dữ liệu.

2. **Hardware Edge Node (`FirmwareESP32/`)**
   - Written in C++ (PlatformIO).
   - Được viết bằng C++ sử dụng PlatformIO.
   - Acts as a TCP Client, periodically sampling hardware data (dummy temperature) and transmitting it as structured payloads (e.g., `[TEMP:28]`) to the Linux Gateway.
   - Hoạt động như một TCP Client, định kỳ lấy mẫu dữ liệu phần cứng (nhiệt độ giả lập) và gửi dưới dạng payload có cấu trúc, ví dụ `[TEMP:28]`, đến Linux Gateway.

##  Build & Run Instructions

### 1. Run the Linux Gateway
### 1. Chạy Linux Gateway
```bash
cd linux_server
make
./gateway_app
```

The server will start and listen on port 8080.

Server sẽ khởi động và lắng nghe kết nối tại port 8080.
