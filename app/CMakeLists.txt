# app - the Qt Widgets controller GUI.

qt_add_executable(VirtualCamera
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/SystemExtensionActivator.cpp
    src/SystemExtensionActivator.h)

set_target_properties(VirtualCamera PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.dynamsoft.virtualcamera"
    MACOSX_BUNDLE_BUNDLE_NAME "VirtualCamera"
    MACOSX_BUNDLE_EXECUTABLE_NAME "VirtualCamera"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/app/Info.plist.in")

target_link_libraries(VirtualCamera PRIVATE
    vcamcore
    Qt6::Widgets)

if(APPLE)
    target_sources(VirtualCamera PRIVATE
        src/SystemExtensionActivator.mm)
    set_source_files_properties(src/SystemExtensionActivator.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(VirtualCamera PRIVATE
        "-framework SystemExtensions"
        "-framework Foundation")
endif()

# Linux: the .deb installs the binary into /usr/bin via
# linux/scripts/make-deb.sh (cmake --install with DESTDIR staging).
if(NOT APPLE AND NOT WIN32)
    install(TARGETS VirtualCamera RUNTIME DESTINATION bin)
endif()
