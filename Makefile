TARGET = rpc
SRCS = rpc.c

all: $(TARGET)

$(TARGET):
	gcc -Wall $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) && rm -f messages.txt
