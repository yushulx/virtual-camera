# windows/driver - the DirectShow capture source filter DLL.
#
# Built on the standard DirectShow base classes (the same approach as the
# proven softcam / classic vcam implementations): CSource + CSourceStream
# handle pin negotiation, allocator setup and the streaming thread; the
# filter code only provides the media type, the frame data and the
# IKsPropertySet pin-category query browsers require.

set(VCAM_BASECLASSES
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses/amfilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses/amvideo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses/combase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses/dllentry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses/dllsetup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses/mtype.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses/source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses/winutil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses/wxdebug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses/wxlist.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses/wxutil.cpp)

add_library(virtualcamera_driver MODULE
    ${VCAM_BASECLASSES}
    src/dllmain.cpp
    src/guids.cpp
    src/SharedFrameReader.cpp
    src/VirtualCamera.cpp
    VirtualCameraDriver.def)

set_target_properties(virtualcamera_driver PROPERTIES
    OUTPUT_NAME "VirtualCameraDriver"
    PREFIX "")

target_include_directories(virtualcamera_driver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/baseclasses
    ${CMAKE_SOURCE_DIR}/core/include)

target_link_libraries(virtualcamera_driver PRIVATE
    strmiids   # DirectShow interface IIDs (IID_IPin, IID_IBaseFilter, ...)
    ole32
    oleaut32
    user32
    advapi32
    winmm)

# Static CRT (/MT): do not require any VC++ runtime in the host process.
set_target_properties(virtualcamera_driver PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded")
