add_executable(virtual-camera-cli src/main.cpp)

target_link_libraries(virtual-camera-cli PRIVATE
    vcamcore
    Qt6::Gui)
