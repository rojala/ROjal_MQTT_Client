include(../CMakeTestServer.txt)
include_directories(../unity
                    ../../include)

add_executable(test_statemaschine test_statemaschine.c)
target_link_libraries (test_statemaschine LINK_PUBLIC unity ROjal_MQTT)
add_test(StateMaschine ${EXECUTABLE_OUTPUT_PATH}/test_statemaschine)
