# 编译器设置
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread -g
LDFLAGS := -pthread -lsqlite3 -lyaml-cpp 

# JSON库路径
JSON_INC := -I./include

# 源文件
SRCS := $(wildcard *.cpp)
MAIN_SRCS := main.cpp  # 主程序入口文件
TEST_SRCS := test_endpoit.cpp  # 测试程序入口文件
COMMON_SRCS := $(filter-out $(MAIN_SRCS) $(TEST_SRCS), $(SRCS))

# 创建build目录
BUILD_DIR := build

# 目标文件
COMMON_OBJS := $(addprefix $(BUILD_DIR)/, $(COMMON_SRCS:.cpp=.o))
MAIN_OBJ := $(BUILD_DIR)/main.o
TEST_OBJ := $(BUILD_DIR)/test_endpoit.o
DEPS := $(COMMON_OBJS:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)

# 目标可执行文件
TARGET := protocol_converter
TEST_TARGET := test

# 默认目标
all: $(TARGET) $(TEST_TARGET)

# 确保build目录存在
$(shell mkdir -p $(BUILD_DIR))

# 链接主程序 - 修复1: 将LDFLAGS放在最后
$(TARGET): $(MAIN_OBJ) $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# 链接测试程序 - 修复1: 将LDFLAGS放在最后
$(TEST_TARGET): $(TEST_OBJ) $(COMMON_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# 编译规则
$(BUILD_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(JSON_INC) -MMD -MP -c $< -o $@

# 包含依赖关系
-include $(DEPS)

# 清理
clean:
	rm -f $(TARGET) $(TEST_TARGET) $(COMMON_OBJS) $(MAIN_OBJ) $(TEST_OBJ) $(DEPS)
	rmdir $(BUILD_DIR) 2>/dev/null || true

# 运行主程序
run: $(TARGET)
	./$(TARGET)

# 运行测试程序
test-run: $(TEST_TARGET)
	./$(TEST_TARGET)

.PHONY: all clean run test-run