# 编译器设置
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread -O2 #-g 
LDFLAGS := -pthread -lsqlite3 -lyaml-cpp 

# 头文件路径 (指向inc目录)
INC := -I./inc

# 源文件路径
SRC_DIR := src
TEST_DIR := tests

# 源文件
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
MAIN_SRCS := $(SRC_DIR)/main.cpp  # 主程序入口文件
TEST_SRCS := $(TEST_DIR)/test_endpoit.cpp  # 测试程序入口文件
COMMON_SRCS := $(filter-out $(MAIN_SRCS), $(SRCS))  # 公共代码

# 创建build目录及其子目录
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
$(shell mkdir -p $(OBJ_DIR)/$(SRC_DIR))
$(shell mkdir -p $(OBJ_DIR)/$(TEST_DIR))

# 目标文件
COMMON_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/$(SRC_DIR)/%.o,$(COMMON_SRCS))
MAIN_OBJ := $(OBJ_DIR)/$(SRC_DIR)/main.o
TEST_OBJ := $(OBJ_DIR)/$(TEST_DIR)/test_endpoit.o
DEPS := $(COMMON_OBJS:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)

# 目标可执行文件
TARGET := protocol_converter
TEST_TARGET := test

# 默认目标
all: $(TARGET) $(TEST_TARGET)

# 链接主程序
$(TARGET): $(MAIN_OBJ) $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# 链接测试程序
$(TEST_TARGET): $(TEST_OBJ) $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# 编译规则 (添加自动目录创建)
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INC) -MMD -MP -c $< -o $@

# 包含依赖关系
-include $(DEPS)

# 清理
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET) $(TEST_TARGET)

# 运行主程序
run: $(TARGET)
	./$(TARGET)

# 运行测试程序
test-run: $(TEST_TARGET)
	./$(TEST_TARGET)

.PHONY: all clean run test-run