# 编译器设置
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread  -O2   #-g 
LDFLAGS := -pthread -lsqlite3 -lyaml-cpp -lpaho-mqttpp3 -lpaho-mqtt3a  

# 头文件路径 (指向inc目录)
INC := -I./inc

# 源文件路径
SRC_DIR := src
TEST_DIR := tests

# 源文件
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
MAIN_SRCS := $(SRC_DIR)/main.cpp  # 主程序入口文件
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
COMMON_SRCS := $(filter-out $(MAIN_SRCS), $(SRCS))  # 公共代码

# 创建构建目录
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
$(shell mkdir -p $(OBJ_DIR)/$(SRC_DIR))
$(shell mkdir -p $(OBJ_DIR)/$(TEST_DIR))

# 目标文件
COMMON_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/$(SRC_DIR)/%.o,$(COMMON_SRCS))
MAIN_OBJ := $(OBJ_DIR)/$(SRC_DIR)/main.o
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp, $(OBJ_DIR)/$(TEST_DIR)/%.o, $(TEST_SRCS))
DEPS := $(COMMON_OBJS:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJS:.o=.d)

# 目标可执行文件
TARGET := protocol_converter
TEST_TARGETS := $(patsubst $(TEST_DIR)/%.cpp, %, $(TEST_SRCS))

# 默认目标
all: $(TARGET) $(TEST_TARGETS)

# 链接主程序
$(TARGET): $(MAIN_OBJ) $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# 链接测试程序 (每个测试生成独立可执行文件)
%: $(OBJ_DIR)/$(TEST_DIR)/%.o $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# 编译规则
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INC) -MMD -MP -c $< -o $@

# 包含依赖关系
-include $(DEPS)

# 清理
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET) $(TEST_TARGETS)

# 运行主程序
run: $(TARGET)
	./$(TARGET)

# 运行所有测试
test-all: $(TEST_TARGETS)
	@echo "========= 运行所有测试 ========="
	@for test in $^; do \
		echo "运行测试: $$test"; \
		./$$test; \
		echo "-----------------------------------"; \
	done

# 运行单个测试 (例如: make run-test-test_parser)
run-test-%: %
	@echo "运行测试: $<"
	@./$<

.PHONY: all clean run test-all run-test-%