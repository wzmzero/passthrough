CXX := g++
CXXFLAGS := -std=c++17 -Iinclude -O2 -Wall -Wextra
LDFLAGS := -lboost_system -lboost_program_options -pthread

BUILD_DIR := build
TARGET_MAIN := net_channel
TARGET_TEST := test_channel

# 源文件定义
CORE_SRCS := $(wildcard src/*.cpp)
MAIN_SRC := src/main.cpp
TEST_SRC := tests/test_channel.cpp

# 排除main.cpp的核心对象文件
CORE_OBJS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(filter-out $(MAIN_SRC),$(CORE_SRCS)))

# 主程序对象
MAIN_OBJ := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(MAIN_SRC))

# 测试程序对象
TEST_OBJ := $(patsubst tests/%.cpp,$(BUILD_DIR)/%.o,$(TEST_SRC))

all: $(TARGET_MAIN) $(TARGET_TEST)

$(TARGET_MAIN): $(CORE_OBJS) $(MAIN_OBJ)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(TARGET_TEST): $(CORE_OBJS) $(TEST_OBJ)
	$(CXX) $^ -o $@ $(LDFLAGS)

# 通用编译规则
$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: tests/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET_MAIN) $(TARGET_TEST)

.PHONY: all clean