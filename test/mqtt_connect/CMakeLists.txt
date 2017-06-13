
if(DEFINED ENV{MQTT_SERVER})
    message("MQTT SERVER is set to: $ENV{MQTT_SERVER}")
    add_definitions(-DMQTT_SERVER="$ENV{MQTT_SERVER}")
else()
    message(FATAL_ERROR "MQTT Broker environment variable not set MQTT_SERVER (use: export MQTT_SERVER=123.456.789.001)")
endif()

include_directories(../unity
                    ../../include)

add_executable(mqtt_connect_simple_tests test_mqtt_connect_simple.c)
target_link_libraries (mqtt_connect_simple_tests LINK_PUBLIC unity ROjal_MQTT)
add_test(MqttSimpleConnect ${EXECUTABLE_OUTPUT_PATH}/mqtt_connect_simple_tests)

