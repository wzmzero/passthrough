# 编译器设置
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread
LIBS = -lboost_system

# 目录结构
ROOT_DIR = .
COMMON_DIR = $(ROOT_DIR)/common
SERVER_DIR = $(ROOT_DIR)/server
CLIENT_DIR = $(ROOT_DIR)/client
BIN_DIR = bin

# 包含路径
INCLUDES = -I$(ROOT_DIR) -I$(COMMON_DIR) -I$(SERVER_DIR) -I$(CLIENT_DIR)

# 源文件
SERVER_SRCS = $(SERVER_DIR)/tcp_server.cpp $(SERVER_DIR)/server_main.cpp
CLIENT_SRCS = $(CLIENT_DIR)/tcp_client.cpp $(CLIENT_DIR)/client_main.cpp

# 目标文件
SERVER_OBJS = $(addprefix $(BIN_DIR)/, tcp_server.o server_main.o)
CLIENT_OBJS = $(addprefix $(BIN_DIR)/, tcp_client.o client_main.o)

# 可执行文件
SERVER_EXE = $(BIN_DIR)/server
CLIENT_EXE = $(BIN_DIR)/client

# 默认目标
all: prepare $(SERVER_EXE) $(CLIENT_EXE)

# 创建bin目录
prepare:
	@mkdir -p $(BIN_DIR)

# 服务器编译
$(SERVER_EXE): $(SERVER_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

# 客户端编译
$(CLIENT_EXE): $(CLIENT_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

# 通用编译规则
$(BIN_DIR)/%.o: $(SERVER_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/%.o: $(CLIENT_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 清理
clean:
	rm -rf $(BIN_DIR)

.PHONY: all prepare clean