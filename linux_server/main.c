#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sqlite3.h> 
#include <sys/epoll.h>
#include <fcntl.h>

#define ERR_EXIT(msg) do { perror(msg); exit(EXIT_FAILURE); } while(0)
#define PORT 8080
#define MAX_ITEMS 5
#define MAX_EVENTS 10 // Phục vụ tối đa 10 sự kiện cùng lúc
int sensor_data = 0;
int data_ready = 0;

// Khóa an toàn và Chuông báo
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t data_cond = PTHREAD_COND_INITIALIZER; 

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int init_tcp_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        ERR_EXIT("[Loi] Khong the tao socket");
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        ERR_EXIT("[Loi] Bind that bai");
    }
    if (listen(server_fd, 3) < 0) {
        ERR_EXIT("[Loi] Listen that bai");
    }
    return server_fd;
}

// Luồng 1: Producer - Lắng nghe TCP và Bấm chuông
void* tcp_producer_thread(void* arg) {
    (void)arg;
    int server_fd = init_tcp_server(PORT);
    set_nonblocking(server_fd); // Ép server không được block
    
    // 1. Tạo sổ ghi chép epoll (Cái quầy của thằng phục vụ)
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("[Loi] Khong tao duoc epoll");
        pthread_exit(NULL);
    }

    // 2. Gắn chuông cho cái cửa chính (Server Socket)
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN; // Báo chuông khi có data hoặc kết nối mới đến
    event.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

    printf("[TCP Server] Dang lang nghe tai Port %d bang EPOLL...\n", PORT);

    while (1) { // Chạy vĩnh viễn
        // 3. Đứng chờ chuông kêu. Hàm này block 0% CPU.
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        for (int i = 0; i < num_events; i++) {
            // TRƯỜNG HỢP A: Chuông kêu ở Cửa chính -> Có ESP32 mới xin vào
            if (events[i].data.fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
                
                if (client_fd > 0) {
                    set_nonblocking(client_fd);
                    
                    // Gắn chuông cho thằng ESP32 này và ghi vào sổ
                    event.events = EPOLLIN | EPOLLET; // Edge-Triggered
                    event.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
                    
                    printf("[Epoll] Khach moi (ESP32) IP %s da ket noi!\n", inet_ntoa(client_addr.sin_addr));
                }
            } 
            // TRƯỜNG HỢP B: Chuông kêu ở Bàn khách -> ESP32 gửi data
            else {
                int client_fd = events[i].data.fd;
                char buffer[128];
                int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
                
                if (bytes_read <= 0) {
                    // Khách bỏ về hoặc rớt mạng -> Xóa tên khỏi sổ
                    printf("[Epoll] Client ngat ket noi. Xoa khoi he thong.\n");
                    close(client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                } else {
                    buffer[bytes_read] = '\0';
                    int new_temp = 0;
                    if (sscanf(buffer, "[TEMP:%d]", &new_temp) == 1) {
                        
                        // Khóa Mutex -> Đẩy Data -> Rung Chuông cho Consumer -> Nhả Mutex
                        pthread_mutex_lock(&data_mutex);
                        sensor_data = new_temp;
                        data_ready = 1;
                        printf("[Producer] Nhan tu FD %d: %d do C\n", client_fd, sensor_data);
                        pthread_cond_signal(&data_cond); 
                        pthread_mutex_unlock(&data_mutex);
                        
                    }
                }
            }
        }
    }
    
    close(server_fd);
    return NULL;
}

// Luồng 2: Consumer - Ngủ sâu chờ chuông và Ghi Database
void* consumer_thread(void* arg) {
    (void)arg;
    sqlite3 *db;
    char *err_msg = 0;
    
    // Mở file Database
    if (sqlite3_open("gateway_data.db", &db) != SQLITE_OK) {
        fprintf(stderr, "[Loi] Khong the mo DB: %s\n", sqlite3_errmsg(db));
        pthread_exit(NULL); 
    }

    // Khởi tạo bảng
    const char *sql_create = "CREATE TABLE IF NOT EXISTS SensorData(Id INTEGER PRIMARY KEY AUTOINCREMENT, Temp INT);";
    sqlite3_exec(db, sql_create, 0, 0, &err_msg);

    int processed = 0;
    while (processed < MAX_ITEMS) {
        pthread_mutex_lock(&data_mutex);
        
        // Ngủ sâu, chờ tín hiệu từ Producer
        while (data_ready == 0) {
            pthread_cond_wait(&data_cond, &data_mutex);
        }

        // Đã có data -> Xử lý ghi DB
        char sql_insert[128];
        snprintf(sql_insert, sizeof(sql_insert), "INSERT INTO SensorData(Temp) VALUES(%d);", sensor_data);
        
        if (sqlite3_exec(db, sql_insert, 0, 0, &err_msg) == SQLITE_OK) {
            printf("[Consumer] Da luu %d do C vao SQLite!\n", sensor_data);
            printf("----------------------------------------\n");
        }

        data_ready = 0;
        processed++;
        pthread_mutex_unlock(&data_mutex);
    }
    
    sqlite3_close(db);
    printf("[Consumer] Dong Database an toan.\n");
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