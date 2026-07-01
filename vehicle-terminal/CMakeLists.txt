cmake_minimum_required(VERSION 3.10)

project(vehicle-terminal LANGUAGES C CXX)

set(QT_VERSION_MAJOR 5)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/forms)

# 检测编译架构
if(CMAKE_CXX_COMPILER MATCHES "arm-poky-linux-gnueabi-g\\+\\+")
    message(STATUS "检测到 ARM 交叉编译工具链")
    set(IS_ARM_CROSS_COMPILE TRUE)
else()
    message(STATUS "本地编译 (x86_64)")
    set(IS_ARM_CROSS_COMPILE FALSE)
endif()

# 查找 Qt
find_package(Qt5 REQUIRED COMPONENTS
    Widgets
    VirtualKeyboard
    Network
    SerialPort
    SerialBus
    Multimedia
)

# 获取所有源文件
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "src/*.h")
file(GLOB_RECURSE UI_FILES CONFIGURE_DEPENDS "src/ui/forms/*.ui")
qt5_add_resources(RESOURCES src/resources/resources.qrc)

# 添加可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${UI_FILES} ${RESOURCES})

# 添加头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/pages
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared
    ${CMAKE_CURRENT_BINARY_DIR}
)

# 添加链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Widgets
    Qt5::VirtualKeyboard
    Qt5::Network
    Qt5::SerialPort
    Qt5::SerialBus
    Qt5::Multimedia
)

# ==================== 测试程序 ====================
option(BUILD_TESTS "构建测试程序" ON)

if(BUILD_TESTS)
    add_executable(test_ec20manager
        tests/test_ec20manager.cpp
        src/core/ec20manager.cpp
        src/config/configmanager.cpp
    )
    target_include_directories(test_ec20manager PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config
    )
    target_link_libraries(test_ec20manager PRIVATE
        Qt5::Core
        Qt5::Network
        Qt5::SerialPort
    )
endif()