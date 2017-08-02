include(../CMakeTestServer.txt)
include_directories(../unity
                    ../../include)

add_executable(mqtt_connect_simple_tests test_mqtt_connect_simple.c)
target_link_libraries (mqtt_connect_simple_tests LINK_PUBLIC unity ROjal_MQTT)
add_test(MqttSimpleConnect ${EXECUTABLE_OUTPUT_PATH}/mqtt_connect_simple_tests)

