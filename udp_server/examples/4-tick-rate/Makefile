# Makefile

CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -O2
TARGET = main
SRC = main.cpp
PORT = 12345

all: kill-port $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@

run: all
	./$(TARGET)

kill-port:
	@PID=$$(lsof -ti udp:$(PORT)); \
	if [ -n "$$PID" ]; then \
		echo "Освобождаю порт $(PORT) (PID=$$PID)"; \
		kill -9 $$PID; \
	else \
		echo "Порт $(PORT) свободен"; \
	fi

clean:
	rm -f $(TARGET)
