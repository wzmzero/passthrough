TARGETS = native arm aarch64 #win 
SRC_SERVER = test_server.cpp NetworkServer.cpp ConfigReader.cpp
SRC_CLIENT = test_client.cpp NetworkClient.cpp  Serial.cpp ConfigReader.cpp

TEST_FILES := test_%.cpp unittest_%.cpp
SRC_PASSTHROUGH := $(filter-out $(TEST_FILES), $(wildcard *.cpp))
CFLAGS = -g -Wall -Wextra #-static #-O2

CC_native = g++
SUFFIX_native = _native

CC_arm = arm-linux-gnueabihf-g++
SUFFIX_arm = _arm

CC_aarch64 = aarch64-linux-gnu-g++
SUFFIX_aarch64 = _aarch64

# 通用规则：为每个平台构建server和client
$(TARGETS):
	@echo "==> Building $@"
	@mkdir -p build
	$(CC_$@) $(CFLAGS) -o build/server$(SUFFIX_$@) $(SRC_SERVER) -lsqlite3
	$(CC_$@) $(CFLAGS) -o build/client$(SUFFIX_$@) $(SRC_CLIENT) -lsqlite3
	$(CC_$@) $(CFLAGS) -o build/passthrough$(SUFFIX_$@) $(SRC_PASSTHROUGH) -lsqlite3 -lpthread
# all 目标：遍历所有平台，调用它们目标  
.PHONY: all clean $(TARGETS)

all:
	@for t in $(TARGETS); do $(MAKE) $$t; done

clean:
	rm -f build/*
	rmdir build 2>/dev/null || true