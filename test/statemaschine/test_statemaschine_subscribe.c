
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>   //inet_addr
#include <netinet/tcp.h> // TCP_NODELAY
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "mqtt.h"
#include "unity.h"
#include "../help/help.h"

extern bool g_auto_state_connection_completed_;
extern bool g_auto_state_subscribe_completed_;
extern bool socket_OK_;

void test_sm_subscrbe()
{
    uint8_t buffer[1024];
    MQTT_shared_data_t shared;

    shared.buffer            = buffer;
    shared.buffer_size       = sizeof(buffer);
    shared.out_fptr          = &data_stream_out_fptr_;
    shared.connected_cb_fptr = &connected_cb_;
    shared.subscribe_cb_fptr = &subscrbe_cb_;

    g_auto_state_connection_completed_ = false;
    g_auto_state_subscribe_completed_  = false;

    MQTT_action_data_t action;
    action.action_argument.shared_ptr = &shared;
    MQTTErrorCodes_t state = mqtt(ACTION_INIT,
                                  &action);

    MQTT_connect_t connect_params;
    uint8_t clientid[] = "JAMKtest test_sm_subscrbe";
    uint8_t aparam[]   = "\0";

    connect_params.client_id                    = clientid;
    connect_params.last_will_topic              = aparam;
    connect_params.last_will_message            = aparam;
    connect_params.username                     = aparam;
    connect_params.password                     = aparam;
    connect_params.keepalive                    = 0;
    connect_params.connect_flags.clean_session  = true;
    connect_params.connect_flags.last_will_qos  = 0;
    connect_params.connect_flags.permanent_will = false;

    action.action_argument.connect_ptr = &connect_params;

    state = mqtt(ACTION_CONNECT,
                 &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    // Wait response and request parse for it
    // Parse will call given callback which will set global flag to true
    int rcv = data_stream_in_fptr_(buffer, sizeof(MQTT_fixed_header_t));

    if (0 < rcv) {
        MQTT_input_stream_t input;
        input.data         = buffer;
        input.size_of_data = (uint32_t)rcv;

        action.action_argument.input_stream_ptr = &input;
        state = mqtt(ACTION_PARSE_INPUT_STREAM,
                     &action);
    } else {
        TEST_ASSERT(0);
    }

    do {
        /* Wait callback */
        asleep(1);
    } while( false == g_auto_state_connection_completed_ );

    const char topic[]     = "test/msg";

    MQTT_subscribe_t subscribe;
    subscribe.topic_ptr    = NULL;
    subscribe.topic_length = 0;
    subscribe.qos          = QoS0;
    subscribe.topic_ptr    = (uint8_t*) topic;
    subscribe.topic_length = strlen(topic);

    action.action_argument.subscribe_ptr = &subscribe;

    state = mqtt(ACTION_SUBSCRIBE,
                 &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    state = mqtt(ACTION_DISCONNECT,
                 NULL);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    // Wait response and request parse for it
    // Parse will call given callback which will set global flag to true
    rcv = 0;
    while (0 == rcv) {
        rcv = data_stream_in_fptr_(buffer, sizeof(MQTT_fixed_header_t));
        if (0 == rcv)
            asleep(10);
    }

    MQTT_input_stream_t input;
    input.data         = buffer;
    input.size_of_data = (uint32_t)rcv;

    action.action_argument.input_stream_ptr = &input;
    state = mqtt(ACTION_PARSE_INPUT_STREAM,
                &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    while (false == g_auto_state_subscribe_completed_)
        asleep(10);

    close_mqtt_socket_();
}


/****************************************************************************************
 * TEST main                                                                            *
 ****************************************************************************************/
int main(void)
{
    UnityBegin("State Maschine");
    unsigned int tCntr = 1;
    RUN_TEST(test_sm_subscrbe,                              tCntr++);
    return (UnityEnd());
}
