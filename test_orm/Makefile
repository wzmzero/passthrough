# 编译器设置
CXX := g++
CXXFLAGS := -std=c++17 -Iinclude -Wall -O2
LDFLAGS := -lsqlite3

# 源文件和目标
SRC_DIR := src
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:.cpp=.o)
TARGET := sqlite_orm_demo

# 安装依赖检查
SQLITE_CHECK := $(shell dpkg -l | grep libsqlite3-dev || echo "not-installed")

# 默认目标
all: check_dep $(TARGET)

# 检查依赖
check_dep:
ifeq ($(SQLITE_CHECK), not-installed)
	$(error "libsqlite3-dev is not installed. Run: sudo apt install libsqlite3-dev")
endif

# 链接可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# 编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(OBJS) $(TARGET) multi_object.db

# 运行程序
run: $(TARGET)
	./$(TARGET)

 
.PHONY: all clean run check_dep  