# core - shared controller side code, linked by the GUI and the CLI.

add_library(vcamcore STATIC)

target_include_directories(vcamcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_sources(vcamcore PRIVATE
    include/vcam/CameraController.h
    include/vcam/VideoSource.h
    src/CameraController.cpp
    src/Config.cpp
    src/FolderSource.cpp
    src/ImageSource.cpp
    src/PatternSource.cpp
    src/ShmFrameWriter.cpp
    src/VideoSource.cpp
    src/YuyvConverter.cpp)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(vcamcore PRIVATE src/V4L2Writer.cpp)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Multimedia)
target_link_libraries(vcamcore PUBLIC Qt6::Core Qt6::Gui Qt6::Multimedia)
