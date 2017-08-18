include(../CMakeTestServer.txt)
include_directories(../unity
                    ../../include
					../socket_read_write_lib)

add_executable(mvp_test mvp_test.c)
target_link_libraries (mvp_test LINK_PUBLIC unity ROjal_MQTT ROjal_MQTT_SOCKET_IF)
add_test(MVPTest ${EXECUTABLE_OUTPUT_PATH}/mvp_test)