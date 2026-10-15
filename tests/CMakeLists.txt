add_executable(vcam-tests src/test_core.cpp)

target_link_libraries(vcam-tests PRIVATE
    vcamcore
    Qt6::Core
    Qt6::Gui)

add_test(NAME core COMMAND vcam-tests)
set_tests_properties(core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
