#include "mqtt.h"
#include "unity.h"

#include<stdio.h>
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>
#include<string.h>
#include <signal.h>
#include<time.h>     //nanosleep

extern MQTTErrorCodes_t mqtt_connect_parse_ack(uint8_t * a_message_in_ptr);
static volatile int g_socket_desc = -1;
static volatile bool g_auto_state_connection_completed = false;
static volatile bool g_auto_state_subscribe_completed = false;

void sleep_ms(int milliseconds)
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

volatile bool socket_OK = false;

void sigpipe_handler()
{
    printf("Socket signal\n");
    socket_OK = false;
}

int open_mqtt_socket()
{
    int socket_desc;
    struct sockaddr_in server;

	// instal sigpipe handler
	signal(SIGPIPE, sigpipe_handler);

    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    TEST_ASSERT_NOT_EQUAL( -1, socket_desc);

	socket_OK = true;
	
    server.sin_addr.s_addr = inet_addr(MQTT_SERVER);
    server.sin_family = AF_INET;
    server.sin_port = htons(MQTT_PORT);

    //Connect to remote server
    TEST_ASSERT_TRUE_MESSAGE(connect(socket_desc,
                                     (struct sockaddr *)&server, 
                                     sizeof(server)
                                     ) >= 0, 
                                     "MQTT Broker not running?");

    struct timeval tv;
    tv.tv_sec  = 10;  /* 5 Secs Timeout */
    tv.tv_usec = 0;  // Not init'ing this can cause strange errors
    setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval));

    return socket_desc;
}


int close_mqtt_socket()
{
	shutdown(g_socket_desc, 2 /* Ignore and stop both RCV and SEND */); //http://www.gnu.org/software/libc/manual/html_node/Closing-a-Socket.html
	while(socket_OK) {
		struct timespec ts;
		ts.tv_sec = 100 / 1000;
		ts.tv_nsec = (100 % 1000) * 1000000;
		nanosleep(&ts, NULL);
		char data = 0;
		if( send(g_socket_desc, &data, 0 , 0) < 0)
			socket_OK = false;
	}
	g_socket_desc = -1;
}

int data_stream_in_fptr(uint8_t * a_data_ptr, size_t a_amount)
{
    if (g_socket_desc <= 0)
        g_socket_desc = open_mqtt_socket();

    if (g_socket_desc > 0) {
        int ret = recv(g_socket_desc, a_data_ptr, a_amount, 0);
        printf("Socket in  -> %iB\n", ret);
        return ret;
    } else {
        return -1;
    }
}

int data_stream_out_fptr(uint8_t * a_data_ptr, size_t a_amount)
{
    if (g_socket_desc <= 0)
        g_socket_desc = open_mqtt_socket();

    if (g_socket_desc > 0) {
        int ret = send(g_socket_desc, a_data_ptr, a_amount, 0);
        printf("Socket out -> %iB\n", ret);
        return ret;
    } else {
        return -1;
    }
}

void connected_cb(MQTTErrorCodes_t a_status)
{
    if (Successfull == a_status)
        printf("Connected CB SUCCESSFULL\n");
    else
        printf("Connected CB FAIL %i\n", a_status);
    g_auto_state_connection_completed = true;
}

void subscrbe_cb(MQTTErrorCodes_t a_status, 
				 uint8_t * a_data_ptr,
				 uint32_t a_data_len,
				 uint8_t * a_topic_ptr,
				 uint16_t a_topic_len)
{
	if (Successfull == a_status) {
        printf("Subscribed CB SUCCESSFULL\n");
		for (int i = 0; i < a_topic_len; i++)
			printf("%c", a_topic_ptr[i]);
		printf(": ");
		for (int i = 0; i < a_data_len; i++)
			printf("%c", a_data_ptr[i]);
		printf("\n");
    } else {
        printf("Subscribed CB FAIL %i\n", a_status);
	}
	g_auto_state_subscribe_completed = true;
}

void test_sm_connect_manual_ack()
{
    uint8_t buffer[256];
    MQTT_shared_data_t shared;
    
    shared.buffer = buffer;
    shared.buffer_size = sizeof(buffer);
    shared.out_fptr = &data_stream_out_fptr;


    MQTT_action_data_t action;
    action.action_argument.shared_ptr = &shared;
    MQTTErrorCodes_t state = mqtt(ACTION_INIT,
                                  &action);

    MQTT_connect_t connect_params;
    
	uint8_t clientid[] = "JAMKtest test_sm_connect_manual_ack";
	uint8_t aparam[] = "\0";
	
    connect_params.client_id = clientid;
    connect_params.last_will_topic = aparam;
    connect_params.last_will_message = aparam;
    connect_params.username = aparam;
    connect_params.password = aparam;
    connect_params.keepalive = 0;
    connect_params.connect_flags.clean_session = true;

    action.action_argument.connect_ptr = &connect_params;

    state = mqtt(ACTION_CONNECT,
                 &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    sleep_ms(400);
    
    // Wait response from borker
    int rcv = data_stream_in_fptr(buffer, sizeof(MQTT_fixed_header_t));
    if (0 < rcv) {
        state = mqtt_connect_parse_ack(buffer);
    } else
    {
        state = NoConnection;
    }
    TEST_ASSERT_EQUAL_INT(Successfull, state);
    shared.state = STATE_CONNECTED;

    state = mqtt(ACTION_DISCONNECT,
                 NULL);

    TEST_ASSERT_EQUAL_INT(Successfull, state);
    
	close_mqtt_socket();
}

void test_sm_connect_auto_ack()
{
    uint8_t buffer[256];
    MQTT_shared_data_t shared;
    
    shared.buffer = buffer;
    shared.buffer_size = sizeof(buffer);
    shared.out_fptr = &data_stream_out_fptr;
    shared.connected_cb_fptr = &connected_cb;
    g_auto_state_connection_completed = false;

    MQTT_action_data_t action;
    action.action_argument.shared_ptr = &shared;
    MQTTErrorCodes_t state = mqtt(ACTION_INIT,
                                  &action);

    MQTT_connect_t connect_params;
	uint8_t clientid[] = "JAMKtest test_sm_connect_auto_ack";
	uint8_t aparam[] = "\0";
	
    connect_params.client_id = clientid;
    connect_params.last_will_topic = aparam;
    connect_params.last_will_message = aparam;
    connect_params.username = aparam;
    connect_params.password = aparam;
    connect_params.keepalive = 0;
    connect_params.connect_flags.clean_session = true;

    action.action_argument.connect_ptr = &connect_params;

    state = mqtt(ACTION_CONNECT,
                 &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    // Wait response and request parse for it
    // Parse will call given callback which will set global flag to true
    int rcv = data_stream_in_fptr(buffer, sizeof(MQTT_fixed_header_t));
    if (0 < rcv) {
        MQTT_input_stream_t input;
        input.data = buffer;
        input.size_of_data = (uint32_t)rcv;
        action.action_argument.input_stream_ptr = &input;

        state = mqtt(ACTION_PARSE_INPUT_STREAM,
                     &action);
    } else {
        TEST_ASSERT(0);
    }

    do {
        /* Wait callback */
        sleep_ms(10);
    } while( false == g_auto_state_connection_completed );

    state = mqtt(ACTION_DISCONNECT,
                 NULL);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

	close_mqtt_socket();
}

void test_sm_connect_auto_ack_keepalive()
{
    uint8_t buffer[256];
    MQTT_shared_data_t shared;
    
    shared.buffer = buffer;
    shared.buffer_size = sizeof(buffer);
    shared.out_fptr = &data_stream_out_fptr;
    shared.connected_cb_fptr = &connected_cb;
    g_auto_state_connection_completed = false;

    MQTT_action_data_t action;
    action.action_argument.shared_ptr = &shared;
    MQTTErrorCodes_t state = mqtt(ACTION_INIT,
                                  &action);

    MQTT_connect_t connect_params;
	uint8_t clientid[] = "JAMKtest test_sm_connect_auto_ack_keepalive";
	uint8_t aparam[] = "\0";
	
    connect_params.client_id = clientid;
    connect_params.last_will_topic = aparam;
    connect_params.last_will_message = aparam;
    connect_params.username = aparam;
    connect_params.password = aparam;
    connect_params.keepalive = 2;
    connect_params.connect_flags.clean_session = true;

    action.action_argument.connect_ptr = &connect_params;

    state = mqtt(ACTION_CONNECT,
                 &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    // Wait response and request parse for it
    // Parse will call given callback which will set global flag to true
    int rcv = data_stream_in_fptr(buffer, sizeof(MQTT_fixed_header_t));
    if (0 < rcv) {
        MQTT_input_stream_t input;
        input.data = buffer;
        input.size_of_data = (uint32_t)rcv;
        action.action_argument.input_stream_ptr = &input;

        state = mqtt(ACTION_PARSE_INPUT_STREAM,
                     &action);
    } else {
        TEST_ASSERT(0);
    }

    do {
        /* Wait callback */
        sleep_ms(1);
    } while( false == g_auto_state_connection_completed );

    MQTT_action_data_t ap;
    ap.action_argument.epalsed_time_in_ms = 500;
    state = mqtt(ACTION_KEEPALIVE, &ap);
    printf("Keepalive cmd status %i\n", state);
    ap.action_argument.epalsed_time_in_ms = 500;

    for (uint8_t i = 0; i < 10; i++)
    {
        printf("g_shared_data->state %u\n", shared.state);

        if (Successfull == state) {
            int rcv = data_stream_in_fptr(buffer, sizeof(MQTT_fixed_header_t));
            if (0 < rcv) {
                MQTT_input_stream_t input;
                input.data = buffer;
                input.size_of_data = (uint32_t)rcv;
                action.action_argument.input_stream_ptr = &input;

                state = mqtt(ACTION_PARSE_INPUT_STREAM,
                            &action);
                TEST_ASSERT_EQUAL_INT(Successfull, state);
            } else {
                printf("no ping resp\n");
                state = Successfull;
                //TEST_ASSERT(0);
            }
        }
        
        printf("sleeping..\n");
        sleep_ms(500);
        printf("slept\n");
        state = mqtt(ACTION_KEEPALIVE, &ap);
        //TEST_ASSERT_EQUAL_INT(Successfull, state);
        
    }
    state = mqtt(ACTION_DISCONNECT,
                 NULL);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

	close_mqtt_socket();
}


void test_sm_publish()
{
    uint8_t buffer[256];
    MQTT_shared_data_t shared;
    
    shared.buffer = buffer;
    shared.buffer_size = sizeof(buffer);
    shared.out_fptr = &data_stream_out_fptr;
    shared.connected_cb_fptr = &connected_cb;
    g_auto_state_connection_completed = false;

    MQTT_action_data_t action;
    action.action_argument.shared_ptr = &shared;
    MQTTErrorCodes_t state = mqtt(ACTION_INIT,
                                  &action);

    MQTT_connect_t connect_params;
	uint8_t clientid[] = "JAMKtest test_sm_publish";
	uint8_t aparam[] = "\0";
	
    connect_params.client_id = clientid;
    connect_params.last_will_topic = aparam;
    connect_params.last_will_message = aparam;
    connect_params.username = aparam;
    connect_params.password = aparam;
    connect_params.keepalive = 2;
    connect_params.connect_flags.clean_session = true;

    action.action_argument.connect_ptr = &connect_params;

    state = mqtt(ACTION_CONNECT,
                 &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    // Wait response and request parse for it
    // Parse will call given callback which will set global flag to true
    int rcv = 0;
    while(rcv == 0) {
        rcv = data_stream_in_fptr(buffer, sizeof(MQTT_fixed_header_t));
        if (rcv == 0)
            sleep_ms(10);
    }

    if (0 < rcv) {
        MQTT_input_stream_t input;
        input.data = buffer;
        input.size_of_data = (uint32_t)rcv;
        action.action_argument.input_stream_ptr = &input;

        state = mqtt(ACTION_PARSE_INPUT_STREAM,
                     &action);
    } else {
        TEST_ASSERT(0);
    }

    do {
        /* Wait callback */
        sleep_ms(1);
    } while( false == g_auto_state_connection_completed );

    MQTT_publish_t publish;
    publish.flags.dup = false;
    publish.flags.retain = false;
    publish.flags.qos = QoS0;

    const char topic[] = "test/msg";
    publish.topic_ptr = (uint8_t*) topic;
    publish.topic_length = strlen(topic);

    const   char message[] = "FooBarMessage";

    publish.message_buffer_ptr = (uint8_t*)message;
    publish.message_buffer_size = strlen(message);

    hex_print((uint8_t *) publish.message_buffer_ptr, publish.message_buffer_size);

    action.action_argument.publish_ptr = &publish;

    state = mqtt(ACTION_PUBLISH,
                 &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    state = mqtt(ACTION_DISCONNECT,
                 NULL);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

	close_mqtt_socket();
}


void test_sm_subscrbe()
{
    uint8_t buffer[256];
    MQTT_shared_data_t shared;
    
    shared.buffer = buffer;
    shared.buffer_size = sizeof(buffer);
    shared.out_fptr = &data_stream_out_fptr;
    shared.connected_cb_fptr = &connected_cb;
	shared.subscribe_cb_fptr = &subscrbe_cb;
    g_auto_state_connection_completed = false;
	g_auto_state_subscribe_completed = false;

    MQTT_action_data_t action;
    action.action_argument.shared_ptr = &shared;
    MQTTErrorCodes_t state = mqtt(ACTION_INIT,
                                  &action);

    MQTT_connect_t connect_params;
	uint8_t clientid[] = "JAMKtest test_sm_subscrbe";
	uint8_t aparam[] = "\0";
	
    connect_params.client_id = clientid;
    connect_params.last_will_topic = aparam;
    connect_params.last_will_message = aparam;
    connect_params.username = aparam;
    connect_params.password = aparam;
    connect_params.keepalive = 0;
    connect_params.connect_flags.clean_session = true;

    action.action_argument.connect_ptr = &connect_params;

    state = mqtt(ACTION_CONNECT,
                 &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    // Wait response and request parse for it
    // Parse will call given callback which will set global flag to true
    int rcv = 0;
    while(rcv == 0) {
        rcv = data_stream_in_fptr(buffer, sizeof(MQTT_fixed_header_t));
        if (rcv == 0)
            sleep_ms(10);
    }

    if (0 < rcv) {
        MQTT_input_stream_t input;
        input.data = buffer;
        input.size_of_data = (uint32_t)rcv;
        action.action_argument.input_stream_ptr = &input;

        state = mqtt(ACTION_PARSE_INPUT_STREAM,
                     &action);
    } else {
        TEST_ASSERT(0);
    }

    do {
        /* Wait callback */
        sleep_ms(1);
    } while( false == g_auto_state_connection_completed );

    MQTT_subscribe_t subscribe;
    subscribe.topic_ptr = NULL;
    subscribe.topic_length = 0;
    subscribe.qos = QoS0;
	
    const char topic[] = "test/msg";
    subscribe.topic_ptr = (uint8_t*) topic;
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
    while(rcv == 0) {
        rcv = data_stream_in_fptr(buffer, sizeof(MQTT_fixed_header_t));
        if (rcv == 0)
            sleep_ms(10);
    }
	
	MQTT_input_stream_t input;
	input.data = buffer;
	input.size_of_data = (uint32_t)rcv;
	action.action_argument.input_stream_ptr = &input;

	state = mqtt(ACTION_PARSE_INPUT_STREAM,
				&action);
	TEST_ASSERT_EQUAL_INT(Successfull, state);
	
	while( g_auto_state_subscribe_completed == false)
		sleep_ms(10);

	close_mqtt_socket();
}


void test_sm_subscrbe_with_receive()
{
    uint8_t buffer[256];
    MQTT_shared_data_t shared;
    
    shared.buffer = buffer;
    shared.buffer_size = sizeof(buffer);
    shared.out_fptr = &data_stream_out_fptr;
    shared.connected_cb_fptr = &connected_cb;
	shared.subscribe_cb_fptr = &subscrbe_cb;
    g_auto_state_connection_completed = false;
	g_auto_state_subscribe_completed = false;

    MQTT_action_data_t action;
    action.action_argument.shared_ptr = &shared;
    MQTTErrorCodes_t state = mqtt(ACTION_INIT,
                                  &action);

    MQTT_connect_t connect_params;
	uint8_t clientid[] = "JAMKtest test_sm_subscrbe_with_receive";
	uint8_t aparam[] = "\0";
	
    connect_params.client_id = clientid;
    connect_params.last_will_topic = aparam;
    connect_params.last_will_message = aparam;
    connect_params.username = aparam;
    connect_params.password = aparam;
    connect_params.keepalive = 0;
    connect_params.connect_flags.clean_session = true;

    action.action_argument.connect_ptr = &connect_params;

    state = mqtt(ACTION_CONNECT,
                 &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    // Wait response and request parse for it
    // Parse will call given callback which will set global flag to true
    int rcv = 0;
    while(rcv == 0) {
        rcv = data_stream_in_fptr(buffer, sizeof(MQTT_fixed_header_t));
        if (rcv == 0)
            sleep_ms(10);
    }

    if (0 < rcv) {
        MQTT_input_stream_t input;
        input.data = buffer;
        input.size_of_data = (uint32_t)rcv;
        action.action_argument.input_stream_ptr = &input;

        state = mqtt(ACTION_PARSE_INPUT_STREAM,
                     &action);
    } else {
        TEST_ASSERT(0);
    }

    do {
        /* Wait callback */
        sleep_ms(1);
    } while( false == g_auto_state_connection_completed );

    MQTT_subscribe_t subscribe;
    subscribe.topic_ptr = NULL;
    subscribe.topic_length = 0;
    subscribe.qos = QoS0;
	
    const char topic[] = "test/msg";
    subscribe.topic_ptr = (uint8_t*) topic;
    subscribe.topic_length = strlen(topic);

    action.action_argument.subscribe_ptr = &subscribe;

    state = mqtt(ACTION_SUBSCRIBE,
                 &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);

    // Wait response and request parse for it
    // Parse will call given callback which will set global flag to true
	rcv = 0;
    while(rcv == 0) {
        rcv = data_stream_in_fptr(buffer, sizeof(MQTT_fixed_header_t));
        if (rcv == 0)
            sleep_ms(10);
    }
	
	MQTT_input_stream_t input;
	input.data = buffer;
	input.size_of_data = (uint32_t)rcv;
	action.action_argument.input_stream_ptr = &input;

	state = mqtt(ACTION_PARSE_INPUT_STREAM,
				&action);
	TEST_ASSERT_EQUAL_INT(Successfull, state);
	
	while( g_auto_state_subscribe_completed == false)
		sleep_ms(10);

	MQTT_publish_t publish;
    publish.flags.dup = false;
    publish.flags.retain = false;
    publish.flags.qos = QoS0;

    publish.topic_ptr = (uint8_t*) topic;
    publish.topic_length = strlen(topic);

    const   char message[] = "FooBarMessage2";

    publish.message_buffer_ptr = (uint8_t*)message;
    publish.message_buffer_size = strlen(message);

    hex_print((uint8_t *) publish.message_buffer_ptr, publish.message_buffer_size);

    action.action_argument.publish_ptr = &publish;

	g_auto_state_subscribe_completed = false;

    state = mqtt(ACTION_PUBLISH,
                 &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);
	
	/* wait publish to come back from broker */
	rcv = 0;
    while(rcv == 0) {
        rcv = data_stream_in_fptr(buffer, sizeof(MQTT_fixed_header_t));
        if (rcv == 0)
            sleep_ms(10);
    }

	g_auto_state_subscribe_completed == false;

	input.data = buffer;
	input.size_of_data = (uint32_t)rcv;
	action.action_argument.input_stream_ptr = &input;

	state = mqtt(ACTION_PARSE_INPUT_STREAM,
				&action);
	
	TEST_ASSERT_EQUAL_INT(Successfull, state);
	
	while(g_auto_state_subscribe_completed == false)
		sleep_ms(10);

    state = mqtt(ACTION_DISCONNECT,
                 NULL);

    TEST_ASSERT_EQUAL_INT(Successfull, state);
	
	close_mqtt_socket();
}
/****************************************************************************************
 * TEST main                                                                            *
 ****************************************************************************************/
int main(void)
{  
    UnityBegin("State Maschine");
    unsigned int tCntr = 1;

    RUN_TEST(test_sm_connect_manual_ack,                    tCntr++);
    RUN_TEST(test_sm_connect_auto_ack,                      tCntr++);
	RUN_TEST(test_sm_connect_auto_ack_keepalive,            tCntr++);
    RUN_TEST(test_sm_publish,                               tCntr++);
	RUN_TEST(test_sm_subscrbe,                              tCntr++);
	RUN_TEST(test_sm_subscrbe_with_receive,                 tCntr++);
    return (UnityEnd());
}
