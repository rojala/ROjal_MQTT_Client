#include "mqtt.h"
#include "unity.h"

#include<stdio.h>
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>
#include<string.h>

/* Functions not declared in mqtt.h - internal functions */
extern uint8_t encode_fixed_header(MQTT_fixed_header_t * output,
                                   bool dup,
                                   MQTTQoSLevel_t qos,
                                   bool retain,
                                   MQTTMessageType_t messageType,
                                   uint32_t msgSize);

extern uint8_t * decode_fixed_header(uint8_t * a_input_ptr,
                                     bool * a_dup_ptr,
                                     MQTTQoSLevel_t * a_qos_ptr,
                                     bool * a_retain_ptr,
                                     MQTTMessageType_t * a_message_type_ptr,
                                     uint32_t * a_message_size_ptr);

extern uint8_t encode_variable_header_connect(uint8_t * a_output_ptr, 
                                              bool a_clean_session,
                                              bool a_last_will,
                                              MQTTQoSLevel_t a_last_will_qos,
                                              bool a_permanent_last_will,
                                              bool a_password,
                                              bool a_username,
                                              uint16_t a_keepalive);

extern uint8_t * decode_variable_header_conack(uint8_t * a_input_ptr, uint8_t * a_connection_state_ptr);

extern MQTTErrorCodes_t mqtt_connect(uint8_t * a_message_buffer_ptr, 
                                     size_t a_max_buffer_size,
                                     data_stream_in_fptr_t a_in_fptr,
                                     data_stream_out_fptr_t a_out_fptr,
                                     MQTT_connect_t * a_connect_ptr,
                                     bool wait_and_parse_response);

extern MQTTErrorCodes_t mqtt_disconnect(data_stream_out_fptr_t a_out_fptr);

MQTTErrorCodes_t mqtt_ping_req(data_stream_out_fptr_t a_out_fptr);

MQTTErrorCodes_t mqtt_parse_ping_ack(uint8_t * a_message_in_ptr);

char buffer[1024*256];

static int g_socket_desc = -1;

int open_mqtt_socket()
{
    int socket_desc;
    struct sockaddr_in server;

    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    TEST_ASSERT_NOT_EQUAL( -1, socket_desc);

    server.sin_addr.s_addr = inet_addr(MQTT_SERVER);
    server.sin_family = AF_INET;
    server.sin_port = htons(MQTT_PORT);

    printf("MQTT server %s:%i\n", MQTT_SERVER, MQTT_PORT);

    //Connect to remote server
    TEST_ASSERT_TRUE_MESSAGE(connect(socket_desc,
                                     (struct sockaddr *)&server, 
                                     sizeof(server)
                                     ) >= 0, 
                                     "MQTT Broker not running?");

    struct timeval tv;
    tv.tv_sec = 30;  /* 30 Secs Timeout */
    tv.tv_usec = 0;  // Not init'ing this can cause strange errors
    setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval));

    return socket_desc;
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


void test_mqtt_socket()
{
    int socket_desc = open_mqtt_socket();
    //Connect to remote server
    TEST_ASSERT_TRUE_MESSAGE(socket_desc >= 0, "MQTT Broker not running?");

    close(socket_desc);
}

void test_mqtt_connect_simple_hack()
{
    // Connect to broker with socket
    int socket_desc = open_mqtt_socket();
    TEST_ASSERT_TRUE_MESSAGE(socket_desc >= 0, "MQTT Broker not running?");

    uint8_t mqtt_raw_buffer[256];
    memset(mqtt_raw_buffer, 0, sizeof(mqtt_raw_buffer));
    uint8_t sizeOfFixedHdr, sizeOfVarHdr;
    MQTT_fixed_header_t fHdr;
    // Form fixed header for CONNECT msg. Message size is count to be 13.
    sizeOfFixedHdr = encode_fixed_header((MQTT_fixed_header_t*)&mqtt_raw_buffer, false, QoS0, false, CONNECT, 13);

    // Form variable header with MVP paramerters.
    sizeOfVarHdr = encode_variable_header_connect(&(mqtt_raw_buffer[sizeOfFixedHdr]), false, false, QoS0, false, false, false, 0);

    // Set device id into payload (size 1 and content o)
    mqtt_raw_buffer[sizeOfFixedHdr + sizeOfVarHdr + 1] = 1;
    mqtt_raw_buffer[sizeOfFixedHdr + sizeOfVarHdr + 2] = 'o';

    // Send CONNECT message to the broker
    TEST_ASSERT_FALSE_MESSAGE(send(socket_desc, mqtt_raw_buffer, sizeOfFixedHdr + sizeOfVarHdr + 3, 0) < 0, "Send failed");

    // Wait response from borker
    int rcv = recv(socket_desc, mqtt_raw_buffer , 2000 , 0);
    TEST_ASSERT_FALSE_MESSAGE(rcv < 0,  "Receive failed with error");
    TEST_ASSERT_FALSE_MESSAGE(rcv == 0, "No data received");

    bool dup, retain;
    MQTTQoSLevel_t qos = 2;
    MQTTMessageType_t type;
    uint32_t size;
    // Decode fixed header
    uint8_t * nextHdr = decode_fixed_header(mqtt_raw_buffer, &dup, &qos, &retain, &type, &size);
    TEST_ASSERT_TRUE(NULL != nextHdr);
    TEST_ASSERT_TRUE(CONNACK == type);

    // Decode variable header
    uint8_t a_connection_state_ptr = 0;
    decode_variable_header_conack(nextHdr, &a_connection_state_ptr);
    TEST_ASSERT_EQUAL_MESSAGE(0, a_connection_state_ptr, "Connection attempt returned failure");

    // Form and send fixed header with DISCONNECT command ID
    sizeOfFixedHdr = encode_fixed_header((MQTT_fixed_header_t*)&mqtt_raw_buffer, false, QoS0, false, DISCONNECT, 0);
    TEST_ASSERT_FALSE_MESSAGE(send(socket_desc, mqtt_raw_buffer, sizeOfFixedHdr, 0) < 0, "Send failed");

    // Close socket
    close(socket_desc);
}

void test_mqtt_connect_simple()
{
    // Connect to broker with socket
    g_socket_desc = open_mqtt_socket();
    TEST_ASSERT_TRUE_MESSAGE(g_socket_desc >= 0, "MQTT Broker not running?");

    uint8_t mqtt_raw_buffer[256];
    MQTT_connect_t connect_params;
    
    sprintf((char*)(connect_params.client_id), "JAMKtest");
    
    connect_params.last_will_topic[0] = '\0';
    connect_params.last_will_message[0] = '\0';
    connect_params.username[0] = '\0';
    connect_params.password[0] = '\0';
    connect_params.keepalive = 0;
    connect_params.connect_flags.clean_session = true;
    MQTTErrorCodes_t ret = mqtt_connect(mqtt_raw_buffer,
                                        sizeof(mqtt_raw_buffer),
                                        &data_stream_in_fptr,
                                        &data_stream_out_fptr,
                                        &connect_params,
                                        true);

    TEST_ASSERT_EQUAL_MESSAGE(0, ret, "MQTT Connect failed");

    // Form and send fixed header with DISCONNECT command ID
    uint8_t sizeOfFixedHdr = encode_fixed_header((MQTT_fixed_header_t*)&mqtt_raw_buffer, false, QoS0, false, DISCONNECT, 0);
    TEST_ASSERT_FALSE_MESSAGE(send(g_socket_desc, mqtt_raw_buffer, sizeOfFixedHdr, 0) < 0, "Send failed");

    // Close socket
    close(g_socket_desc);
    g_socket_desc = -1;
}

void test_mqtt_connect_simple_username_and_password()
{
    // Connect to broker with socket
    g_socket_desc = open_mqtt_socket();
    TEST_ASSERT_TRUE_MESSAGE(g_socket_desc >= 0, "MQTT Broker not running?");

    uint8_t mqtt_raw_buffer[256];
    MQTT_connect_t connect_params;
    
    sprintf((char*)(connect_params.client_id), "JAMKtest");
    
    connect_params.last_will_topic[0] = '\0';
    connect_params.last_will_message[0] = '\0';
    sprintf((char*)(connect_params.username),"aUser");
    sprintf((char*)(connect_params.password),"aPassword");
    connect_params.keepalive = 0;
    connect_params.connect_flags.clean_session = true;
    connect_params.connect_flags.permanent_will = false;
    MQTTErrorCodes_t ret = mqtt_connect(mqtt_raw_buffer,
                                        sizeof(mqtt_raw_buffer),
                                        &data_stream_in_fptr,
                                        &data_stream_out_fptr,
                                        &connect_params,
                                        true);

    TEST_ASSERT_FALSE_MESSAGE(ret != 0, "MQTT Connect failed");

    // Form and send fixed header with DISCONNECT command ID
    uint8_t sizeOfFixedHdr = encode_fixed_header((MQTT_fixed_header_t*)&mqtt_raw_buffer, false, QoS0, false, DISCONNECT, 0);
    TEST_ASSERT_FALSE_MESSAGE(send(g_socket_desc, mqtt_raw_buffer, sizeOfFixedHdr, 0) < 0, "Send failed");

    // Close socket
    close(g_socket_desc);
    g_socket_desc = -1;
}

void test_mqtt_connect_simple_all_details()
{
    // Connect to broker with socket
    g_socket_desc = open_mqtt_socket();
    TEST_ASSERT_TRUE_MESSAGE(g_socket_desc >= 0, "MQTT Broker not running?");

    uint8_t mqtt_raw_buffer[256];
    MQTT_connect_t connect_params;
    
    sprintf((char*)(connect_params.client_id), "JAMKtest");
    sprintf((char*)(connect_params.last_will_topic),"/IoT/device/state");
    sprintf((char*)(connect_params.last_will_message),"Offline");
    sprintf((char*)(connect_params.username),"aUser");
    sprintf((char*)(connect_params.password),"aPassword");
    connect_params.keepalive = 60;
    connect_params.connect_flags.clean_session = true;
    connect_params.connect_flags.permanent_will = false;
    MQTTErrorCodes_t ret = mqtt_connect(mqtt_raw_buffer,
                                        sizeof(mqtt_raw_buffer),
                                        &data_stream_in_fptr,
                                        &data_stream_out_fptr,
                                        &connect_params,
                                        true);

    TEST_ASSERT_FALSE_MESSAGE(ret != 0, "MQTT Connect failed");

    // MQTT disconnect
    TEST_ASSERT_TRUE(mqtt_disconnect(&data_stream_out_fptr) == Successfull);

    // Close socket
    close(g_socket_desc);
    g_socket_desc = -1;
}

void test_mqtt_connect_simple_keepalive()
{
    // Connect to broker with socket
    g_socket_desc = open_mqtt_socket();
    TEST_ASSERT_TRUE_MESSAGE(g_socket_desc >= 0, "MQTT Broker not running?");

    uint8_t mqtt_raw_buffer[256];
    MQTT_connect_t connect_params;
    
    sprintf((char*)(connect_params.client_id), "JAMKtest");
    
    connect_params.last_will_topic[0] = '\0';
    connect_params.last_will_message[0] = '\0';
    connect_params.username[0] = '\0';
    connect_params.password[0] = '\0';
    connect_params.keepalive = 2;
    connect_params.connect_flags.clean_session = true;
    MQTTErrorCodes_t ret = mqtt_connect(mqtt_raw_buffer,
                                        sizeof(mqtt_raw_buffer),
                                        &data_stream_in_fptr,
                                        &data_stream_out_fptr,
                                        &connect_params,
                                        true);

    TEST_ASSERT_FALSE_MESSAGE(ret != 0, "MQTT Connect failed");

    for (uint8_t i=0; i<4; i++) {
        uint8_t mqtt_raw_buffer[64];
        sleep(1);
        // MQTT PING REQ
        TEST_ASSERT_TRUE(mqtt_ping_req(&data_stream_out_fptr) == Successfull);

        // Wait PINGRESP from borker
        int rcv = recv(g_socket_desc, mqtt_raw_buffer , 2000 , 0);
        TEST_ASSERT_FALSE_MESSAGE(rcv < 0,  "Receive failed with error");
        TEST_ASSERT_FALSE_MESSAGE(rcv == 0, "No data received");
        TEST_ASSERT_EQUAL(Successfull, mqtt_parse_ping_ack(mqtt_raw_buffer));
    }

    // MQTT disconnect
    TEST_ASSERT_TRUE(mqtt_disconnect(&data_stream_out_fptr) == Successfull);

    // Close socket
    close(g_socket_desc);
    g_socket_desc = -1;
}


void test_mqtt_connect_simple_keepalive_timeout()
{
    // Connect to broker with socket
    g_socket_desc = open_mqtt_socket();
    TEST_ASSERT_TRUE_MESSAGE(g_socket_desc >= 0, "MQTT Broker not running?");

    uint8_t mqtt_raw_buffer[256];
    MQTT_connect_t connect_params;
    
    sprintf((char*)(connect_params.client_id), "JAMKtest");
    
    connect_params.last_will_topic[0] = '\0';
    connect_params.last_will_message[0] = '\0';
    connect_params.username[0] = '\0';
    connect_params.password[0] = '\0';
    connect_params.keepalive = 2;
    connect_params.connect_flags.clean_session = true;
    MQTTErrorCodes_t ret = mqtt_connect(mqtt_raw_buffer,
                                        sizeof(mqtt_raw_buffer),
                                        &data_stream_in_fptr,
                                        &data_stream_out_fptr,
                                        &connect_params,
                                        true);

    TEST_ASSERT_FALSE_MESSAGE(ret != 0, "MQTT Connect failed");

    for (uint8_t i=0; i<2; i++) {
        uint8_t mqtt_raw_buffer[64];
        sleep(1+(i*2));
        // MQTT PING REQ
        TEST_ASSERT_TRUE(mqtt_ping_req(&data_stream_out_fptr) == Successfull);

        // Wait PINGRESP from borker
        int rcv = recv(g_socket_desc, mqtt_raw_buffer , 2000 , 0);
        TEST_ASSERT_FALSE_MESSAGE(rcv < 0,  "Receive failed with error");
        if (i == 0) {
            TEST_ASSERT_FALSE_MESSAGE(rcv == 0, "No data received");
            TEST_ASSERT_EQUAL(Successfull, mqtt_parse_ping_ack(mqtt_raw_buffer));
        } else {
            TEST_ASSERT_TRUE_MESSAGE(rcv == 0, "Data received??");
        }
    }

    // Close socket
    close(g_socket_desc);
    g_socket_desc = -1;
}


/****************************************************************************************
 * TEST main                                                                            *
 ****************************************************************************************/
int main(void)
{  
    UnityBegin("MQTT connect simple");
    unsigned int tCntr = 1;

    /* CONNACK */
    RUN_TEST(test_mqtt_socket,                                  tCntr++);
    RUN_TEST(test_mqtt_connect_simple_hack,                     tCntr++);
    RUN_TEST(test_mqtt_connect_simple,                          tCntr++);
    RUN_TEST(test_mqtt_connect_simple_username_and_password,    tCntr++);
    RUN_TEST(test_mqtt_connect_simple_all_details,              tCntr++);
    RUN_TEST(test_mqtt_connect_simple_keepalive,                tCntr++);
    RUN_TEST(test_mqtt_connect_simple_keepalive_timeout,        tCntr++);

    return (UnityEnd());
}
