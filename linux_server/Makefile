CC = gcc
CFLAGS = -Wall -Wextra -pthread
TARGET = gateway_app

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)