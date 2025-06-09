# Makefile

CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -O2
TARGET = main
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)
