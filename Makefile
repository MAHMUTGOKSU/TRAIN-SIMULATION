# Makefile for compiling simulation.cpp

# Compiler settings
CC = g++
CFLAGS = -Wall -lpthread

# Target executable name
TARGET = simulation

# Build target
all: $(TARGET)

$(TARGET): simulation.cpp
	$(CC) $(CFLAGS) simulation.cpp -o $(TARGET)

# Clean up
clean:
	rm -f $(TARGET) train.log control-center.log

# Phony targets
.PHONY: all clean
