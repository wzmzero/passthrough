# 编译器设置
CXX := g++
CXXFLAGS := -std=c++17 -Iinclude -Wall -O2
LDFLAGS := -lsqlite3 -lyaml-cpp 

# 目录设置
SRC_DIR := src
TESTS_DIR := tests
BUILD_DIR := build

# 源文件
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
TEST_SRCS := $(wildcard $(TESTS_DIR)/*.cpp)

# 目标文件
SRC_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
TEST_OBJS := $(patsubst $(TESTS_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(TEST_SRCS))

# 可执行目标
TARGETS := $(notdir $(basename $(TEST_SRCS)))

# 安装依赖检查
SQLITE_CHECK := $(shell dpkg -l | grep libsqlite3-dev || echo "not-installed")

# 默认目标：检查依赖并构建所有目标
all: check_dep $(BUILD_DIR) $(TARGETS)

# 检查依赖
check_dep:
ifeq ($(SQLITE_CHECK), not-installed)
	$(error "libsqlite3-dev is not installed. Run: sudo apt install libsqlite3-dev")
endif

# 创建构建目录
$(BUILD_DIR):
	mkdir -p $@

# 编译库源文件
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 编译测试源文件
$(BUILD_DIR)/%.o: $(TESTS_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 链接可执行文件
%: $(BUILD_DIR)/%.o $(SRC_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 清理所有生成的文件
clean:
	rm -rf $(TARGETS) $(BUILD_DIR) multi_object.db

# 伪目标声明
.PHONY: all clean check_dep