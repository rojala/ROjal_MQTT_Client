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
 *  The above copyright notice and this permission notice shall be included                                 *
 *  in all copies or substantial portions of the Software.                                                  *
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
#include <string.h>  // memcpy
#include <unistd.h>  // sleep

#define MQTT_MAX_MESSAGE_SIZE                   (0x80000000 - 1)

/**
 * @brief MQTT connection state
 *
 * Two major states, connected and disconnected.
 */
typedef enum MQTTState
{
    STATE_DISCONNECTED = 0,
    STATE_CONNECTED
} MQTTState_t;

/**
 * @brief MQTT action in mqtt() function.
 *
 * Defines action what mqtt() function shall do.
 */
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

/**
 * @brief MQTT message types
 *
 * @ref <a href="linkURLhttp://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.pdf">3.1.1 chapter 2.2.1 MQTT Control Packet type</a>
 */
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

/**
 * @brief MQTT status and error codes
 *
 * Value 0 is handled as successfull operation like in the specification.
 */
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

/**
 * @brief MQTT message types
 *
 * @ref <a href="linkURLhttp://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.pdf">3.1.1 chapter 4.3 Quality of Service levels and protocol flows</a>
 */

typedef enum MQTTQoSLevel
{
    QoS0 = 0,
    QoS1,
    QoS2,
    QoSInvalid
} MQTTQoSLevel_t;


/****************************************************************************************
 * @brief data structures                                                               *
 * Values and bitfields to fill MQTT messages correctly                                 *
 ****************************************************************************************/

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

/* VaARIABLE HEADER */
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

/* CONNECT */
typedef struct MQTT_connect
{
    MQTT_fixed_header_t fixed_header;
    MQTT_variable_header_connect_flags_t connect_flags;
    uint16_t keepalive;
    uint8_t * last_will_topic;
    uint8_t * last_will_message;
    uint8_t * username;
    uint8_t * password;
    uint8_t * client_id;
} MQTT_connect_t;

/****************************************************************************************
 * @brief state and data handling function pointers                                     *
 * Following function pointers are used with connection and subscribe functionalities.  *
 ****************************************************************************************/
typedef void (*connected_fptr_t)(MQTTErrorCodes_t a_status);

typedef void (*subscrbe_fptr_t)(MQTTErrorCodes_t a_status,
                                uint8_t * a_data_ptr,
                                uint32_t a_data_len,
                                uint8_t * a_topic_ptr,
                                uint16_t a_topic_len);


/****************************************************************************************
 * @brief input and output function pointers                                            *
 * Following function pointers are used to send and receive MQTT messages.              *
 * Must be implemnted.                                                                  *
 ****************************************************************************************/
typedef int (*data_stream_in_fptr_t)(uint8_t * a_data_ptr, size_t a_amount);

typedef int (*data_stream_out_fptr_t)(uint8_t * a_data_ptr, size_t a_amount);


/****************************************************************************************
 * @brief shared data structure.                                                        *
 * MQTT stack uses this shared data sructure to keep its state and needed function      *
 * pointers in safe.                                                                    *
 ****************************************************************************************/
typedef struct MQTT_shared_data
{
    MQTTState_t state;                    /* Connection state                           */
    connected_fptr_t connected_cb_fptr;   /* Connected callback                         */
    subscrbe_fptr_t subscribe_cb_fptr;    /* Subscribe callback                         */
    uint8_t * buffer;                     /* Pointer to transmit buffer                 */
    size_t  buffer_size;                  /* Size of transmit buffer                    */
    data_stream_out_fptr_t out_fptr;      /* Sending out MQTT stream function pointer   */
    uint32_t mqtt_packet_cntr;            /* MQTT packet indentifer counter             */
    int32_t keepalive_in_ms;              /* Keepalive timer value - connect message    */
    int32_t time_to_next_ping_in_ms;      /* Keepalive counter to track next ping       */
	bool subscribe_status;                /* Internal subscribe status flag             */
} MQTT_shared_data_t;

/****************************************************************************************
 * @brief MQTT action structures                                                        *
 * MQTT action parameter structures for different actions.                              *
 ****************************************************************************************/
typedef struct MQTT_input_stream
{
    uint8_t  * data;
    uint32_t   size_of_data;
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


/****************************************************************************************
 * @brief MQTT action union                                                             *
 ****************************************************************************************/
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

/****************************************************************************************
 * @brief Debug                                                                         *
 ****************************************************************************************/
/**
 * Debug hex print
 *
 * Dump out given data chunk in hex format
 *
 * @param a_data_ptr [in] pointer to beginning of the chunk.
 * @param a_size [in] size of the chunk in bytes.
 * @return None
 */
void hex_print(uint8_t * a_data_ptr, size_t a_size);

/**
 * mqtt_printf
 *
 * printf function to print out debugging data.
 *
 */
#define mqtt_printf printf

/**
 * mqtt_memcpy
 *
 * memcpy function to copy data from source to destination.
 *
 */
#define mqtt_memcpy memcpy

/**
 * mqtt_memset
 *
 * Set given memory with specified value = memset.
 *
 */
#define mqtt_memset memset

#define mqtt_sleep sleep

#define mqtt_strlen strlen

/****************************************************************************************
 * @brief API                                                                           *
 ****************************************************************************************/
/**
 * mqtt API
 *
 * API function to control, send and receive MQTT messges.
 *
 * @param a_action [in] action type see MQTTAction_t.
 * @param a_action_ptr [in] parameters for action see MQTT_action_data_t.
 * @return None
 */
MQTTErrorCodes_t mqtt(MQTTAction_t a_action,
                      MQTT_action_data_t * a_action_ptr);
                      
bool mqtt_connect(char                   * a_client_name_ptr,
                  uint16_t                 a_keepalive_timeout,
                  uint8_t                * a_username_str_ptr,
                  uint8_t                * a_password_str_ptr,
                  uint8_t                * a_last_will_topic_str_ptr,
                  uint8_t                * a_last_will_str_ptr,
                  MQTT_shared_data_t     * mqtt_shared_data_ptr,
                  uint8_t                * a_output_buffer_ptr,
                  size_t                   a_output_buffer_size,
                  bool                     a_clean_session,
                  data_stream_out_fptr_t   a_out_write_fptr,
                  connected_fptr_t         a_connected_fptr,
                  subscrbe_fptr_t          a_subscribe_fptr,
                  uint8_t                  a_timeout_in_sec);
                  
bool mqtt_disconnect();

bool mqtt_publish(char * a_topic_ptr,
                  size_t a_topic_size,
                  char * a_msg_ptr,
                  size_t a_msg_size);

bool mqtt_subscribe(char               * a_topic,
                    uint8_t              a_timeout_in_sec);

bool mqtt_keepalive(uint32_t a_duration_in_ms);

bool mqtt_receive(uint8_t * a_data, size_t a_amount);

#endif /* MQTT_H */