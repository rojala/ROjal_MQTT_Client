
#include "mqtt.h"
#include "unity.h"

#include<stdio.h>
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>
#include<string.h>
#include<time.h>      //nanosleep

#include "socket_read_write.h"

static uint8_t mqtt_send_buffer[1024*10];
static MQTT_shared_data_t mqtt_shared_data;
static bool g_mqtt_connected = false;
static bool g_mqtt_subscribed = false;
static bool mvp_subscribe_test2 = false;

void sleep_ms(int milliseconds)
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}


void connected_cb(MQTTErrorCodes_t a_status)
{
    if (Successfull == a_status)
        printf("Connected CB SUCCESSFULL\n");
    else
        printf("Connected CB FAIL %i\n", a_status);
	g_mqtt_connected = true;
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
		
		if ( memcmp("mvp/test2", a_topic_ptr, a_topic_len) == 0) {
			if (memcmp("MVP testing", a_data_ptr, a_data_len ) == 0)
				mvp_subscribe_test2 = true;
		}
		
    } else {
        printf("Subscribed CB FAIL %i\n", a_status);
	}
	g_mqtt_subscribed = true;
}


void data_from_socket(uint8_t * a_data, size_t a_amount)
{
	/* Parse input messages */
	MQTT_input_stream_t input;
	input.data = a_data;
	input.size_of_data = (uint32_t)a_amount;
	
	MQTT_action_data_t action;
	action.action_argument.input_stream_ptr = &input;

	MQTTErrorCodes_t state = mqtt(ACTION_PARSE_INPUT_STREAM,
								  &action);

	TEST_ASSERT_EQUAL_INT(Successfull, state);
}

bool enable(uint16_t a_keepalive_timeout, char * clientName)
{
	/* Open socket */
	bool ret = socket_initialize(MQTT_SERVER, MQTT_PORT, data_from_socket);
	
	if (ret) {
		/* Initialize MQTT */
		mqtt_shared_data.buffer = mqtt_send_buffer;
		mqtt_shared_data.buffer_size = sizeof(mqtt_send_buffer);
		mqtt_shared_data.out_fptr = &socket_write;
		mqtt_shared_data.connected_cb_fptr = &connected_cb;
		mqtt_shared_data.subscribe_cb_fptr = &subscrbe_cb;
		
		MQTT_action_data_t action;
		action.action_argument.shared_ptr = &mqtt_shared_data;
		MQTTErrorCodes_t state = mqtt(ACTION_INIT,
									  &action);

		TEST_ASSERT_EQUAL_INT(Successfull, state);
		printf("MQTT Initialized\n");
		
		/* Connect to broker */
		MQTT_connect_t connect_params;
		connect_params.client_id = clientName;
		uint8_t aparam[] = "\0";
		connect_params.last_will_topic = aparam;
		connect_params.last_will_message = aparam;
		connect_params.username = aparam;
		connect_params.password = aparam;
		connect_params.keepalive = a_keepalive_timeout;
		connect_params.connect_flags.clean_session = true;

		action.action_argument.connect_ptr = &connect_params;

		state = mqtt(ACTION_CONNECT,
					 &action);

		TEST_ASSERT_EQUAL_INT(Successfull, state);
		printf("MQTT Connecting\n");

		uint8_t timeout = 50;
		while (timeout != 0 && g_mqtt_connected == false) {
			timeout--;
			sleep_ms(100);
		}
		
		printf("MQTT connack or timeout %i %i\n", g_mqtt_connected, timeout);
		if (g_mqtt_connected == false)
			ret = false;
	}
	g_mqtt_connected = false;
	//sleep_ms(1000);
	return ret;
}

bool disable()
{
	MQTTErrorCodes_t state = mqtt(ACTION_DISCONNECT,
							      NULL);

    TEST_ASSERT_EQUAL_INT(Successfull, state);
	printf("MQTT Disconnected\n");
	
	printf("Close socket\n");
	TEST_ASSERT_TRUE_MESSAGE(stop_reading_thread(), "SOCKET exit failed");
	//sleep_ms(2000);
	return true;
}

bool mvp_publish(char * a_topic, char * a_msg)
{
	MQTT_publish_t publish;
    publish.flags.dup = false;
    publish.flags.retain = false;
    publish.flags.qos = QoS0;

    publish.topic_ptr = (uint8_t*)a_topic;
    publish.topic_length = strlen(a_topic);

    publish.message_buffer_ptr = (uint8_t*)a_msg;
    publish.message_buffer_size = strlen(a_msg);

	MQTT_action_data_t action;
    action.action_argument.publish_ptr = &publish;

    MQTTErrorCodes_t state = mqtt(ACTION_PUBLISH,
								  &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);
	return true;
}

bool mvp_subscribe(char * a_topic)
{
	bool ret = false;
	MQTT_subscribe_t subscribe;
    subscribe.qos = QoS0;
	
    subscribe.topic_ptr = (uint8_t*) a_topic;
    subscribe.topic_length = strlen(a_topic);

	MQTT_action_data_t action;
    action.action_argument.subscribe_ptr = &subscribe;

    MQTTErrorCodes_t state = mqtt(ACTION_SUBSCRIBE,
								  &action);

    TEST_ASSERT_EQUAL_INT(Successfull, state);
	
	uint8_t timeout = 50;
	while (timeout != 0 && g_mqtt_subscribed == false) {
		timeout--;
		sleep_ms(100);
	}
	if (g_mqtt_subscribed == true)
		ret = true;
	
	g_mqtt_subscribed = false;
	
	return ret;
}

bool mvp_keepalive(uint32_t a_duration_in_ms)
{
	MQTT_action_data_t ap;
    ap.action_argument.epalsed_time_in_ms = a_duration_in_ms;
    MQTTErrorCodes_t state = mqtt(ACTION_KEEPALIVE, &ap);
    return true;
}

void mvp_test_a()
{
	TEST_ASSERT_TRUE_MESSAGE(enable(0, "JAMKtestMVP1"), "Connect failed");
	TEST_ASSERT_TRUE_MESSAGE(disable(), "Disconnect failed");
}

void mvp_test_b()
{
	TEST_ASSERT_TRUE_MESSAGE(enable(0, "JAMKtestMVP2"), "Connect failed");
	
	TEST_ASSERT_TRUE_MESSAGE(mvp_publish("mvp/test1", "MVP testing"), "Publish failed");
	sleep_ms(500);
	
	TEST_ASSERT_TRUE_MESSAGE(disable(), "Disconnect failed");
}

void mvp_test_c()
{
	TEST_ASSERT_TRUE_MESSAGE(enable(0, "JAMKtestMVP3"), "Connect failed");
	TEST_ASSERT_TRUE_MESSAGE(mvp_subscribe("mvp/test2"), "Subscribe failed");

	TEST_ASSERT_TRUE_MESSAGE(mvp_publish("mvp/test2", "MVP testing"), "Publish failed");
	sleep_ms(500);
	TEST_ASSERT_TRUE_MESSAGE(mvp_subscribe_test2, "Subscribe test failed - invalid content");
	TEST_ASSERT_TRUE_MESSAGE(disable(), "Disconnect failed");
	mvp_subscribe_test2 = false;
}

void mvp_test_d()
{
	TEST_ASSERT_TRUE_MESSAGE(enable(4, "JAMKtestMVP4"), "Connect failed");

	TEST_ASSERT_TRUE_MESSAGE(mvp_subscribe("mvp/test2"), "Subscribe failed");
	for (int i = 0; i < (60 /* * 60 * 20 */) ; i++) {
		if ( i % 10 == 0) {
			mvp_subscribe_test2 = false;
			TEST_ASSERT_TRUE_MESSAGE(mvp_publish("mvp/test2", "MVP testing"), "Publish failed");
			sleep_ms(1000);
			TEST_ASSERT_TRUE_MESSAGE(mvp_subscribe_test2, "Subscribe test failed - invalid content");
		} else {
			sleep_ms(1000);
		}
		TEST_ASSERT_TRUE_MESSAGE(mvp_keepalive(1000), "keepalive failed")
	}

	TEST_ASSERT_TRUE_MESSAGE(disable(), "Disconnect failed");
}


/****************************************************************************************
 * TEST main                                                                            *
 ****************************************************************************************/
int main(void)
{  
    UnityBegin("MVP tests");
    unsigned int tCntr = 1;

	RUN_TEST(mvp_test_a,                    tCntr++);
	RUN_TEST(mvp_test_b,                    tCntr++);
	RUN_TEST(mvp_test_c,                    tCntr++);
	RUN_TEST(mvp_test_d,                    tCntr++);
    return (UnityEnd());
}
