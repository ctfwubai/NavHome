cmake_minimum_required(VERSION 3.21)

# 导航之家（NavHome）网页管家
# 本地单机 Windows 桌面应用，技术栈 C++17 + Qt6 Widgets + SQLite
project(NavHome
    VERSION 2.0.0
    DESCRIPTION "导航之家（NavHome）网页管家"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 自动处理 moc / uic / rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# MSVC 下统一按 UTF-8 处理源码，保证中文字符串正确
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql Network)

# 收集全部源码（新增文件后重新 cmake 即可自动纳入）
file(GLOB_RECURSE NAVHOME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
# 资源文件（图标等）
file(GLOB_RECURSE NAVHOME_RESOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.qrc"
)

# 是否显示控制台（调试用；默认隐藏为纯 GUI 程序）
option(NAVHOME_CONSOLE "显示控制台窗口（调试用）" OFF)
if(NAVHOME_CONSOLE)
    add_executable(NavHome ${NAVHOME_SOURCES} ${NAVHOME_RESOURCES})
else()
    add_executable(NavHome WIN32 ${NAVHOME_SOURCES} ${NAVHOME_RESOURCES})
endif()

target_include_directories(NavHome PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(NavHome PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
)

if(WIN32)
    # 输出名为 NavHome.exe
    set_target_properties(NavHome PROPERTIES
        OUTPUT_NAME "NavHome"
        WIN32_EXECUTABLE TRUE
    )
    # Windows 应用图标资源：复制图标到构建目录根，再编译 .rc（windres 相对构建目录解析）
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico"
        "${CMAKE_CURRENT_BINARY_DIR}/app.ico"
        COPYONLY
    )
    target_sources(NavHome PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    # 标题栏深色模式（DwmSetWindowAttribute）
    target_link_libraries(NavHome PRIVATE dwmapi)
endif()

# —— 数据层测试（可选，-DNAVHOME_BUILD_TESTS=ON 时构建）——
option(NAVHOME_BUILD_TESTS "构建数据层测试" OFF)
if(NAVHOME_BUILD_TESTS)
    add_executable(NavHomeTest
        tests/test_data.cpp
        src/app/Database.cpp
        src/app/BackupManager.cpp
        src/models/BookmarkRepository.cpp
    )
    target_include_directories(NavHomeTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(NavHomeTest PRIVATE Qt6::Core Qt6::Sql)
endif()
