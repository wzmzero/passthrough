# Makefile for MQTT Duplex Client with CLI11
CXX := g++
CXXFLAGS := -std=c++17 -Wall -O2
LDFLAGS := -lpaho-mqttpp3 -lpaho-mqtt3a
CLI11_INC := /usr/local/include # 根据实际安装路径调整

TARGET := mqtt_client
SRC := mqtt_duplex_param.cpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -I$(CLI11_INC) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)