set(CMAKE_SYSTEM_NAME Linux) # 设置目标系统名字
set(CMAKE_SYSTEM_PROCESSOR arm) # 设置目标处理器架构

# 指定编译器的sysroot路径
set(TOOLCHAIN_DIR /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots)
set(CMAKE_SYSROOT ${TOOLCHAIN_DIR}/cortexa7hf-neon-poky-linux-gnueabi)
set(ENV{PKG_CONFIG_PATH} ${CMAKE_SYSROOT}/usr/lib/pkgconfig)

# 设置 Qt5 路径
set(QT_INSTALL_DIR /home/hajimi/linux/tools/qt/qt5.15.5-arm)
set(CMAKE_PREFIX_PATH ${QT_INSTALL_DIR})
set(Qt5_DIR ${QT_INSTALL_DIR}/lib/cmake/Qt5)
set(QT5_INCLUDE_DIR ${QT_INSTALL_DIR}/include)
set(QT5_LIB_DIR ${QT_INSTALL_DIR}/lib)
set(QMAKE_EXECUTABLE ${QT_INSTALL_DIR}/bin/qmake CACHE FILEPATH "QMake executable")
set(OE_QMAKE_PATH_EXTERNAL_HOST_BINS "${TOOLCHAIN_DIR}/x86_64-pokysdk-linux/usr/bin" CACHE PATH "Path to qmake")

# 指定交叉编译器arm-linux-gcc和arm-linux-g++
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-g++)

# 为编译器添加编译选项
set(CMAKE_C_FLAGS "-march=armv7ve -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a7 --sysroot=${CMAKE_SYSROOT}")
set(CMAKE_CXX_FLAGS "-march=armv7ve -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a7 --sysroot=${CMAKE_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
