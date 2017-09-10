include(../CMakeTestServer.txt)
include_directories(../unity
                    ../../include
					../socket_read_write_lib)

add_executable(prod_test prod_test.c)
target_link_libraries (prod_test LINK_PUBLIC unity ROjal_MQTT ROjal_MQTT_SOCKET_IF)
add_test(PRODTest ${EXECUTABLE_OUTPUT_PATH}/prod_test)