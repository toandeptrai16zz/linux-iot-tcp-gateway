#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define MAX_ITEMS 5

int sensor_data = 0;
int data_ready = 0;
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

// Single Responsibility Principle: Hàm này chỉ làm 1 việc duy nhất là khởi tạo Server
int init_tcp_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("[Loi] Khong the tao socket");
        exit(EXIT_FAILURE); // Never swallow exceptions silently
    }

    // Ép hệ điều hành nhả Port ra ngay lập tức khi tắt app, tránh lỗi "Address already in use"
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[Loi] Bind that bai (Port co the dang bi chiem)");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("[Loi] Listen that bai");
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

// Luồng 1: Producer - Chờ kết nối và đọc data qua TCP
void* tcp_producer_thread(void* arg) {
    (void)arg; // Unused parameter
    int server_fd = init_tcp_server(PORT);
    printf("[TCP Server] Dang lang nghe tai Port %d...\n", PORT);

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Block luồng ở đây cho đến khi ESP32 kết nối vào
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("[Loi] Accept that bai");
        pthread_exit(NULL);
    }
    
    printf("[TCP Server] Client (ESP32) IP %s da ket noi!\n", inet_ntoa(client_addr.sin_addr));

    char buffer[128];
    for (int i = 0; i < MAX_ITEMS; i++) {
        int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) break; // Client ngắt kết nối hoặc lỗi đường truyền

        buffer[bytes_read] = '\0';
        int new_temp = 0;
        
        // Parse data format: [TEMP:28]
        sscanf(buffer, "[TEMP:%d]", &new_temp); 

        pthread_mutex_lock(&data_mutex);
        sensor_data = new_temp;
        data_ready = 1;
        printf("[Producer] Nhan qua Wi-Fi: %d do C\n", sensor_data);
        pthread_mutex_unlock(&data_mutex);
    }
    
    close(client_fd);
    close(server_fd);
    printf("[Producer] Dong ket noi. Hoan thanh nhiem vu!\n");
    return NULL;
}

// Luồng 2: Consumer - Bốc data mang đi xử lý (Logic không đổi)
void* consumer_thread(void* arg) {
    (void)arg; // Unused parameter
    int processed = 0;
    while (processed < MAX_ITEMS) {
        pthread_mutex_lock(&data_mutex);
        if (data_ready == 1) {
            printf("[Consumer] Dong goi JSON day len Cloud: {\"temp\": %d}\n", sensor_data);
            printf("----------------------------------------\n");
            data_ready = 0;
            processed++;
        }
        pthread_mutex_unlock(&data_mutex);
        usleep(500000); 
    }
    return NULL;
}

int main() {
    pthread_t producer, consumer;
    printf("=== BOOTING LINUX IOT GATEWAY ===\n");
    
    pthread_create(&producer, NULL, tcp_producer_thread, NULL);
    pthread_create(&consumer, NULL, consumer_thread, NULL);

    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    
    printf("=== GATEWAY SHUTDOWN ===\n");
    return 0;
}