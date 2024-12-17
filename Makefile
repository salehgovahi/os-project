CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0`
TARGET = OS-PROJECT

all: $(TARGET)

$(TARGET): main.c
	$(CC) -o $(TARGET) main.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
