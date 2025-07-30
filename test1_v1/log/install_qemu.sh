#!/bin/bash

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Install general dependencies
echo -e "${YELLOW}Installing general dependencies...${NC}"
sudo apt-get update
sudo apt-get install -y make g++ build-essential gdb gdb-multiarch qemu-user-static sqlite3 libsqlite3-dev
sudo apt install mingw-w64   # For cross-compilation to Windows
# Install ARM64 cross-compilation tools
echo -e "${YELLOW}Installing ARM64 cross-compilation tools...${NC}"
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu libc6-arm64-cross

# Install ARM32 cross-compilation tools
echo -e "${YELLOW}Installing ARM32 cross-compilation tools...${NC}"
sudo apt-get install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf libc6-dev-armhf-cross

# 验证 ARM64 工具链安装
echo -e "${GREEN}Checking ARM64 toolchain...${NC}"
aarch64-linux-gnu-gcc --version

# 验证 ARM32 工具链安装
echo -e "${GREEN}Checking ARM32 toolchain...${NC}"
arm-linux-gnueabihf-gcc --version