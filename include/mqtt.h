/************************************************************************************************************
 * Copyright 2017 Rami Ojala / JAMK                                                                         *
 *                                                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of                          *
 * this software and associated documentation files (the "Software"), to deal in the                        *
 * Software without restriction, including without limitation the rights to use, copy,                      *
 * modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,                      * 
 * and to permit persons to whom the Software is furnished to do so, subject to the                         *
 * following conditions:                                                                                    *
 *                                                                                                          *
 * 	The above copyright notice and this permission notice shall be included                                 *
 * 	in all copies or substantial portions of the Software.                                                  *
 *                                                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,                      *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A                            *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT                       *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION                        *
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE                           *
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                                   *
 *                                                                                                          *
 * https://opensource.org/licenses/MIT                                                                      *
 ************************************************************************************************************/

#ifndef MQTT_H
#define MQTT_H
#include <stdint.h>  // uint
#include <stddef.h>  // size_t
#include <stdbool.h> // bool
#include <stdio.h>   // printf

#define MQTT_MAX_MESSAGE_SIZE                   (0x80000000 - 1)
#define MQTT_CONNECT_LAST_WILL_TOPIC_SIZE       256
#define MQTT_CONNECT_LAST_WILL_MESSAGE_SIZE     128
#define MQTT_USERNAME_SIZE                      32
#define MQTT_PASSWORD_SIZE                      32
#define MQTT_CLIENT_ID_SIZE                     24

typedef enum MQTTState
{
    STATE_DISCONNECTED = 0,
    STATE_CONNECTED,
    STATE_PINGREQ
} MQTTState_t;

typedef enum MQTTAction
{
    ACTION_DISCONNECT,
    ACTION_CONNECT,
    ACTION_PUBLISH,
    ACTION_SUBSCRIBE,
    ACTION_KEEPALIVE,
    ACTION_INIT,
    ACTION_PARSE_INPUT_STREAM
} MQTTAction_t;

typedef enum MQTTMessageType
{
    INVALIDCMD = 0,
    CONNECT = 1,
    CONNACK,
    PUBLISH,
    PUBACK,
    PUBREC,
    PUBREL,
    PUBCOMP,
    SUBSCRIBE,
    SUBACK,
    UNSUBSCRIBE,
    UNSUBACK,
    PINGREQ,
    PINGRESP,
    DISCONNECT,
    MAXCMD
} MQTTMessageType_t;

typedef enum MQTTErrorCodes
{
    InvalidArgument = -64,
    NoConnection,
    AllreadyConnected,
    PingNotSend,
    Successfull = 0,
    InvalidVersion = 1,
    InvalidIdentifier,
    ServerUnavailabe,
    BadUsernameOrPassword,
    NotAuthorized,
	PublishDecodeError
} MQTTErrorCodes_t;

typedef enum MQTTQoSLevel
{
    QoS0 = 0,
    QoS1,
    QoS2,
    QoSInvalid  
} MQTTQoSLevel_t;

#pragma pack(1)
typedef struct struct_flags_and_type
{
    uint8_t retain:1;      /* Retain or not                            */
    uint8_t dup:1;         /* one bit value, duplicate or not          */
    uint8_t qos:2;         /* Quality of service 0-2 @see MQTTQoSLevel */
    uint8_t message_type:4;/* @see MQTTMessageType                     */
} struct_flags_and_type_t;

/* FIXED HEADER */
typedef struct MQTT_fixed_header
{
    struct_flags_and_type_t flagsAndType;
    uint8_t length[4];
} MQTT_fixed_header_t;

/* Variable headers */
typedef struct MQTT_variable_header_connect_flags
{
    uint8_t reserved:1;
    uint8_t clean_session:1;
    uint8_t last_will:1;
    uint8_t last_will_qos:2;
    uint8_t permanent_will:1;
    uint8_t password:1;
    uint8_t username:1;
} MQTT_variable_header_connect_flags_t;

typedef struct MQTT_variable_header_connect
{
    uint8_t length[2];        /* In practise fixed to 4                    */
    uint8_t procol_name[4];   /* "MQTT" - string without null              */
    uint8_t protocol_version; /* Fixed to 3.1.1 = 0x04                     */
    uint8_t flags;            /* @see MQTT_variable_header_connect_flags_t */
    uint8_t keepalive[2];     /* Keepaive timer for the connection         */
} MQTT_variable_header_connect_t;

typedef struct MQTT_connect
{
    MQTT_fixed_header_t fixed_header;
    MQTT_variable_header_connect_flags_t connect_flags;
    uint16_t keepalive;
    uint8_t last_will_topic[MQTT_CONNECT_LAST_WILL_TOPIC_SIZE];
    uint8_t last_will_message[MQTT_CONNECT_LAST_WILL_MESSAGE_SIZE];
    uint8_t username[MQTT_USERNAME_SIZE];
    uint8_t password[MQTT_PASSWORD_SIZE];
    uint8_t client_id[MQTT_CLIENT_ID_SIZE];
} MQTT_connect_t;

/** 
 * Function pointer,....
 */
typedef void (*connected_fptr_t)(MQTTErrorCodes_t a_status);
typedef void (*subscrbe_fptr_t)(MQTTErrorCodes_t a_status, 
						        uint8_t * a_data_ptr,
								uint32_t a_data_len,
								uint8_t * a_topic_ptr,
								uint16_t a_topic_len);

/** 
 * Function pointer, which is used to read data from input stream 
 */
typedef int (*data_stream_in_fptr_t)(uint8_t * a_data_ptr, size_t a_amount);

/** 
 * Function pointer, which is used to sed data to output stream 
 */
typedef int (*data_stream_out_fptr_t)(uint8_t * a_data_ptr, size_t a_amount);


typedef struct MQTT_shared_data
{
    MQTTState_t state;
    connected_fptr_t connected_cb_fptr;
    subscrbe_fptr_t subscribe_cb_fptr;
    uint8_t * buffer;
    size_t  buffer_size;
    data_stream_out_fptr_t out_fptr;
    uint32_t mqtt_time;
    uint32_t mqtt_packet_cntr;
    int32_t keepalive_in_ms;
    int32_t last_updated_timer_in_ms;
} MQTT_shared_data_t;

typedef struct MQTT_input_stream
{
    uint8_t * data;
    uint32_t size_of_data;
} MQTT_input_stream_t;

typedef struct MQTT_publish
{
    struct_flags_and_type_t flags;
    uint8_t * topic_ptr;
    uint16_t  topic_length;
    uint8_t * message_buffer_ptr;
    uint32_t  message_buffer_size;
} MQTT_publish_t;

typedef struct MQTT_subscribe
{
    MQTTQoSLevel_t qos;
    uint8_t * topic_ptr;
    uint16_t  topic_length;
} MQTT_subscribe_t;

typedef struct MQTT_action_data
{
    union {
        MQTT_shared_data_t * shared_ptr;
        MQTT_connect_t * connect_ptr;
        uint32_t epalsed_time_in_ms;
        MQTT_input_stream_t * input_stream_ptr;
        MQTT_publish_t * publish_ptr;
        MQTT_subscribe_t * subscribe_ptr;
    } action_argument;
} MQTT_action_data_t;

typedef struct MQTT_data_storage
{
    uint8_t * data_start;
    uint8_t * data_ptr;
    uint8_t * data_end;
    uint32_t bytes_in_storage;

} MQTT_data_storage_t;
/**
 * Debug hex print
 *
 * Dump out given data chunk in hex format
 *
 * @param a_data_ptr [in] pointer to beginning of the chunk
 * @param a_size [in] size of the chunk in bytes
 * @return None
 */
void hex_print(uint8_t * a_data_ptr, size_t a_size);

#define mqtt_printf printf
#endif /* MQTT_H */

MQTTErrorCodes_t mqtt(MQTTAction_t a_action,
                      MQTT_action_data_t * a_action_ptr);