# ARM 交叉编译与部署

> 讲解 vehicle-terminal 从 x86 开发机到 ARM Linux 车机的完整构建与部署流程。

---

## 一、构建系统概览

vehicle-terminal 使用 **CMake + Ninja**，通过 **CMakePresets** 管理双平台构建：

```
CMakePresets.json
├── base           (Generator: Ninja)
├── arm-debug      (交叉编译, Yocto SDK)
├── arm-release    (交叉编译, Release)
├── x86-debug      (本地编译)
└── x86-release    (本地编译, 日常开发)
```

### 构建命令

```bash
# x86 本地开发 (日常使用)
cmake --preset x86-release
cmake --build build/x86-release -- -j$(nproc)

# ARM 交叉编译 (部署前)
cmake --preset arm-release
cmake --build build/arm-release --target vehicle-terminal -- -j$(nproc)
```

---

## 二、双平台配置

### 2.1 build/x86-release — 本地开发

```bash
# 依赖检查
sudo apt install qtbase5-dev qt5serialbus-dev libqt5virtualkeyboard5-dev \
                 qtmultimedia5-dev libasound2-dev
```

无 CAN 硬件时使用 vcan0 虚拟接口：

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

### 2.2 build/arm-release — ARM 交叉编译

需要 Yocto Poky SDK（NXP i.MX 系列 BSP）：

```bash
# 1. 安装 SDK (一次性)
/opt/fsl-imx-x11/4.1.15-2.1.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi
# 此脚本设置 CC, CXX, SYSROOT, PKG_CONFIG_PATH 等环境变量

# 2. 安装交叉编译的 Qt 5.15.5
# 位置: /home/hajimi/linux/tools/qt/qt5.15.5-arm

# 3. CMake 配置 → 编译
cmake --preset arm-release
cmake --build build/arm-release --target vehicle-terminal -- -j$(nproc)
```

---

## 三、toolchain.cmake 详解

交叉编译的核心是告诉 CMake："目标平台不是 x86，是 ARMv7"：

```cmake
# vehicle-terminal/tools/toolchain.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Sysroot: ARM 目标文件系统根
set(CMAKE_SYSROOT /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi)

# 交叉编译器
set(CMAKE_C_COMPILER   .../arm-poky-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER .../arm-poky-linux-gnueabi-g++)

# CPU 特性
set(CMAKE_C_FLAGS "-march=armv7ve -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a7 --sysroot=${CMAKE_SYSROOT}")

# 查找策略: 只在 sysroot 中找库, 不在宿主机找
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)    # 用宿主机程序 (cmake 等)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)     # 用目标机库
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)     # 用目标机头文件
```

### 关键参数

| 参数 | 值 | 含义 |
|---|---|---|
| `-march=armv7ve` | ARMv7 + Virtualization Extensions | Cortex-A7 指令集 |
| `-mfpu=neon` | NEON SIMD | 硬件浮点加速 |
| `-mfloat-abi=hard` | hard-float ABI | 浮点参数走 VFP 寄存器 |
| `-mcpu=cortex-a7` | 具体核心 | 编译器优化 + 调度策略 |

### Qt 5.15.5 路径（toolchain.cmake 中配置）

```cmake
set(QT_INSTALL_DIR /home/hajimi/linux/tools/qt/qt5.15.5-arm)
set(CMAKE_PREFIX_PATH ${QT_INSTALL_DIR})
set(Qt5_DIR ${QT_INSTALL_DIR}/lib/cmake/Qt5)
```

CMake 通过 `find_package(Qt5)` 找到交叉编译的 Qt，链接 ARM 版本的 libQt5Widgets.so 等。

---

## 四、自动部署

### 4.1 deploy.sh

```bash
#!/bin/bash
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REMOTE_SERVER="root@192.168.26.48"
REMOTE_DIR="~/vehicle-terminal"
LOCAL_EXECUTABLE="$PROJECT_ROOT/build/arm-release/vehicle-terminal"

# 1. 远程创建目录
ssh $REMOTE_SERVER "mkdir -p $REMOTE_DIR"

# 2. SCP 传输可执行文件
scp "$LOCAL_EXECUTABLE" $REMOTE_SERVER:$REMOTE_DIR/

# 3. SSH 远程启动
ssh -t $REMOTE_SERVER "source /etc/profile && $REMOTE_DIR/vehicle-terminal"
```

### 4.2 部署前提

- ARM 板已配置好 IP (192.168.26.48)
- SSH 免密登录已配置 (`ssh-copy-id`)
- 目标 Qt 运行时库已安装 (`qtbase`, `qtserialbus` 等)

---

## 五、平台差异处理

### 5.1 设备文件

```cpp
// config.h 中的平台差异
#ifdef Q_OS_LINUX
    #ifdef __arm__
        #define CAN_DEVICE_DEFAULT   "can0"
        #define GPS_DEVICE_DEFAULT   "/dev/ttymxc2"
        #define CAMERA_DEVICE_DEFAULT "/dev/video1"
    #else
        #define CAN_DEVICE_DEFAULT   "vcan0"
        #define GPS_DEVICE_DEFAULT   "/dev/ttyUSB0"
        #define CAMERA_DEVICE_DEFAULT "/dev/video1"
    #endif
#endif
```

### 5.2 SocketCAN 初始化

ARM 板上的物理 CAN 接口需要先配置波特率：

```bash
ip link set can0 type can bitrate 250000
ip link set up can0
```

x86 上 vcan0 不需要指定 bitrate。

---

## 六、完整开发-部署工作流

```
1. x86 编码+调试
   cmake --preset x86-release
   cmake --build build/x86-release
   → 在 PC 上用 vcan0 + Mock RTE 跑完整 UI

2. ARM 交叉编译
   cmake --preset arm-release
   cmake --build build/arm-release --target vehicle-terminal -- -j$(nproc)

3. 部署到车机
   ./tools/deploy.sh
   → SCP 传输 → SSH 远端启动 → 接真实 CAN 硬件
```

---

## 七、总结

1. **CMakePresets** 管理双平台——arm-release 和 x86-release 共享相同 CMakeLists.txt
2. **toolchain.cmake** 告诉 CMake 目标不是 x86 而是 ARM Cortex-A7
3. **vcan0** 让开发不需要真实 CAN 硬件
4. **deploy.sh** 一键 SCP + SSH，从编译到运行全自动
5. **平台差异** 集中到 `config.h` 的设备路径宏

## 参考资料

- [CMakePresets.json](../vehicle-terminal/CMakePresets.json)
- [toolchain.cmake](../vehicle-terminal/tools/toolchain.cmake)
- [deploy.sh](../vehicle-terminal/tools/deploy.sh)
- [CMake Cross Compiling Documentation](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
