
#include "mqtt.h"
#include "unity.h"

#include<stdio.h>
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>
#include<string.h>
#include<time.h>      //nanosleep
#include "socket_read_write.h"

static uint8_t mqtt_send_buffer[1024];
static MQTT_shared_data_t mqtt_shared_data;
	  
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
}

void subscrbe_cb(MQTTErrorCodes_t a_status, 
				 uint8_t * a_data_ptr,
				 uint32_t a_data_len,
				 uint8_t * a_topic_ptr,
				 uint16_t a_topic_len)
{
	if (Successfull == a_status) {
		//printf("%s\n",a_data_ptr);
		
        printf("Subscribed CB SUCCESSFULL\n");
		for (uint16_t i = 0; i < a_topic_len; i++)
			printf("%c", a_topic_ptr[i]);
		printf(": ");
		for (uint32_t i = 0; i < a_data_len; i++)
			printf("%c", a_data_ptr[i]);
		printf("\n");
    } else {
        printf("Subscribed CB FAIL %i\n", a_status);
	}
}

void data_from_socket(uint8_t * a_data, size_t a_amount)
{
	TEST_ASSERT_TRUE_MESSAGE(mqtt_receive(a_data, a_amount), "Receive failed (socket -> mqtt)");
}

bool enable(uint16_t a_keepalive_timeout, char * clientName)
{
	/* Open socket */
	bool ret = socket_initialize(MQTT_SERVER, MQTT_PORT, data_from_socket);
	
	if (ret) {
		/* Initialize MQTT */
        return mqtt_connect(clientName,
                            a_keepalive_timeout,
                            (uint8_t*)"\0",
                            (uint8_t*)"\0",
                            (uint8_t*)"\0",
                            (uint8_t*)"\0",
                            &mqtt_shared_data,
                            mqtt_send_buffer,
                            sizeof(mqtt_send_buffer),
                            false,
                            &socket_write,
                            &connected_cb,
                            &subscrbe_cb,
                            10);
	}
	return false;
}

bool disable()
{
    TEST_ASSERT_TRUE_MESSAGE(mqtt_disconnect(), "MQTT Disconnect failed");
	printf("MQTT Disconnected\n");
	
	printf("Close socket\n");
	TEST_ASSERT_TRUE_MESSAGE(stop_reading_thread(), "SOCKET exit failed");
	//sleep_ms(2000);
	return true;
}

void prod_test_a()
{
	TEST_ASSERT_TRUE_MESSAGE(enable(0, "prod_test_a"), "Connect failed");
	TEST_ASSERT_TRUE_MESSAGE(disable(), "Disconnect failed");
}

void prod_test_b()
{
	TEST_ASSERT_TRUE_MESSAGE(enable(0, "prod_test_b"), "Connect failed");
	
	TEST_ASSERT_TRUE_MESSAGE(mqtt_publish("prod/test1", 10, "PROD testing", 12), "Publish failed");
	sleep_ms(500);
	
	TEST_ASSERT_TRUE_MESSAGE(disable(), "Disconnect failed");
}

void prod_test_c()
{
	TEST_ASSERT_TRUE_MESSAGE(enable(0, "prod_test_c"), "Connect failed");
	
	char sub[] = "prod/test2";
	TEST_ASSERT_TRUE_MESSAGE(mqtt_subscribe(sub, strlen(sub), 10), "Subscribe failed");

	TEST_ASSERT_TRUE_MESSAGE(mqtt_publish("prod/test2", 10, "PROD testing", 12), "Publish failed");
	sleep_ms(500);
	TEST_ASSERT_TRUE_MESSAGE(disable(), "Disconnect failed");
}

void prod_test_d()
{
	TEST_ASSERT_TRUE_MESSAGE(enable(4, "prod_test_d"), "Connect failed");
	
	char sub[] = "prod/test2";
	TEST_ASSERT_TRUE_MESSAGE(mqtt_subscribe(sub, strlen(sub), 10), "Subscribe failed");
	for (int i = 0; i < (60 /* * 60 * 20 */) ; i++) {
		if ( i % 10 == 0) {
			TEST_ASSERT_TRUE_MESSAGE(mqtt_publish("prod/test2", 10, "PROD testing", 12), "Publish failed");
			sleep_ms(1000);
		} else {
			sleep_ms(1000);
		}
		TEST_ASSERT_TRUE_MESSAGE(mqtt_keepalive(1000), "keepalive failed")
	}

	TEST_ASSERT_TRUE_MESSAGE(disable(), "Disconnect failed");
}


/****************************************************************************************
 * TEST main                                                                            *
 ****************************************************************************************/
int main(void)
{  
    UnityBegin("Production tests");
    unsigned int tCntr = 1;

	RUN_TEST(prod_test_a,                    tCntr++);
	RUN_TEST(prod_test_b,                    tCntr++);
	RUN_TEST(prod_test_c,                    tCntr++);
	RUN_TEST(prod_test_d,                    tCntr++);
    return (UnityEnd());
}
