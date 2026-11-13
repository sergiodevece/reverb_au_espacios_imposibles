file(GLOB EI_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(ei_tests ${EI_TEST_SOURCES})
target_link_libraries(ei_tests PRIVATE ei_dsp)
add_test(NAME ei_dsp_tests COMMAND ei_tests)
