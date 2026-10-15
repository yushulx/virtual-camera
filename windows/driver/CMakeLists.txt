# windows/driver - the DirectShow capture source filter DLL.
#
# Plain C++ / COM against the Windows SDK: no CRT dependencies beyond the
# static runtime, no Qt - the DLL is loaded into arbitrary applications
# (browsers, conferencing apps) and must be self-contained.

add_library(virtualcamera_driver MODULE
    src/CameraFilter.cpp
    src/CameraPin.cpp
    src/ClassFactory.cpp
    src/EnumMediaTypes.cpp
    src/EnumPins.cpp
    src/guids.cpp
    src/dllmain.cpp
    src/MediaSample.cpp
    src/MediaTypes.cpp
    src/MemAllocator.cpp
    src/Registry.cpp
    src/SharedFrameReader.cpp
    VirtualCameraDriver.def)

set_target_properties(virtualcamera_driver PROPERTIES
    OUTPUT_NAME "VirtualCameraDriver"
    PREFIX "")

target_include_directories(virtualcamera_driver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/core/include)

target_link_libraries(virtualcamera_driver PRIVATE
    ole32
    user32
    advapi32)

# Static CRT (/MT): do not require any VC++ runtime in the host process.
set_target_properties(virtualcamera_driver PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded")
