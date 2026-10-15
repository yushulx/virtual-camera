cmake_minimum_required(VERSION 3.21)

project(virtual-camera
    VERSION 1.0.0
    DESCRIPTION "Virtual camera for Windows, Linux and macOS"
    LANGUAGES CXX)

option(VCAM_BUILD_TESTS "Build the unit tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt: moc the sources automatically.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

message(STATUS "virtual-camera ${PROJECT_VERSION} (Qt ${Qt6Core_VERSION})")

add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(cli)

if(WIN32)
    # The DirectShow capture source filter (registered as a system camera).
    add_subdirectory(windows/driver)
    # Diagnostic tool: enumerates capture devices like a browser does.
    add_executable(enum-cameras tools/enum-cameras.cpp)
    target_link_libraries(enum-cameras PRIVATE
        ole32 oleaut32 advapi32 strmiids)
    target_compile_definitions(enum-cameras PRIVATE UNICODE _UNICODE)
    # Diagnostic tool: runs the capture graph end to end.
    add_executable(graph-test tools/graph-test.cpp)
    target_link_libraries(graph-test PRIVATE
        ole32 oleaut32 strmiids)
    target_compile_definitions(graph-test PRIVATE UNICODE _UNICODE)
endif()

if(VCAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
