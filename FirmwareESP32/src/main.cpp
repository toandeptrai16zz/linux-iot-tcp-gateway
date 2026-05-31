#include <Arduino.h>
#include <WiFi.h>

// Không dùng Magic Strings/Numbers rải rác. Gom hết cấu hình lên đầu.
const char* WIFI_SSID = "538DC0 101 102";
const char* WIFI_PASSWORD = "0989533806";
const char* SERVER_IP = "192.168.0.103"; // Thay bằng IP của máy ảo Linux
const uint16_t SERVER_PORT = 8080;

WiFiClient client;

// SRP: Chỉ chịu trách nhiệm kết nối Wi-Fi
void connect_wifi() {
    Serial.print("[WiFi] Dang ket noi den: ");
    Serial.println(WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[WiFi] Da ket noi! IP cua ESP32: ");
    Serial.println(WiFi.localIP());
}

// SRP: Chỉ chịu trách nhiệm kết nối đến TCP Server của Linux
bool connect_tcp_server() {
    Serial.print("[TCP] Dang ket noi den Server Linux: ");
    Serial.println(SERVER_IP);

    if (client.connect(SERVER_IP, SERVER_PORT)) {
        Serial.println("[TCP] Ket noi thanh cong!");
        return true;
    }
    Serial.println("[TCP] Ket noi that bai. Server chua mo?");
    return false;
}

void setup() {
    Serial.begin(115200);
    
    // Tắt tính năng sleep của Wi-Fi để tránh rớt mạng chập chờn trên ESP32
    WiFi.setSleep(false); 
    
    connect_wifi();
}

void loop() {
    // Xử lý đứt cáp/rớt mạng: Mất Wi-Fi thì tự động nối lại
    if (WiFi.status() != WL_CONNECTED) {
        connect_wifi();
    }

    // Xử lý lỗi Server: Nếu Server Linux sập hoặc chưa bật, luồng này sẽ cố nối lại
    if (!client.connected()) {
        connect_tcp_server();
        delay(2000); // Tránh spam requets liên tục làm treo ESP32 nếu server chết
        return;
    }

    // Sinh dữ liệu giả (Nhiệt độ 20-40 độ C)
    int temp = random(20, 41);
    char payload[32];
    snprintf(payload, sizeof(payload), "[TEMP:%d]", temp);

    // Gắn payload đẩy qua Socket TCP
    client.print(payload);
    
    Serial.print("[TX] Da ban len Linux: ");
    Serial.println(payload);

    delay(1000); // Chu kỳ gửi 1 giây
}