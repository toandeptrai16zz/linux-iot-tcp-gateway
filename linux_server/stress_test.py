import socket
import time
import threading
import random

SERVER_IP = "127.0.0.1"  # Bắn thẳng vào localhost (chính máy ảo)
SERVER_PORT = 8080
NUM_CLIENTS = 200      #  n con ESP32 thì sửa số này

def simulate_esp32(client_id):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((SERVER_IP, SERVER_PORT))
        
        # Mô phỏng node gửi data 5 lần, mỗi lần cách nhau nửa giây
        for _ in range(5): 
            temp = random.randint(20, 40)
            payload = f"[TEMP:{temp}]"
            s.sendall(payload.encode('utf-8'))
            time.sleep(0.5)
            
        s.close()
    except Exception as e:
        print(f"[-] Client {client_id} rớt mạng: {e}")

threads = []
print(f"=== KÍCH HOẠT {NUM_CLIENTS} CON ESP32 ẢO TẤN CÔNG SERVER ===")

# Đẻ ra 100 luồng, mỗi luồng là 1 con ESP32
for i in range(NUM_CLIENTS):
    t = threading.Thread(target=simulate_esp32, args=(i,))
    threads.append(t)
    t.start()

# Chờ 100 con bắn xong
for t in threads:
    t.join()
    
print("==HOÀN THÀNH STRESS TEST ===")