// FIXED HEADER TO OWN HDR FILE ?

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
    Successfull = 0,
    InvalidVersion = 1,
    InvalidIdentifier,
    ServerUnavailabe,
    BadUsernameOrPassword,
    NotAuthorized
} MQTTErrorCodes_t;

typedef enum MQTTQoSLevel
{
    QoS0 = 0,
    QoS1,
    QoS2,
    QoSInvalid  
}MQTTQoSLevel_t;

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
/**
 * Decode connack from variable header stream.
 *
 * Parse out connection status from connak message.
 *
 * @param a_message_buffer_ptr [out] allocated working space
 * @param a_max_buffer_size [in] maximum size of the working space
 * @param a_socket_desc_ptr [in] socket handler
 * @param a_connect_ptr [in] connection parameters @see MQTT_connect_t
 * @param wait_and_parse_response [in] when true, function will wait connak response from the broker and parse it
 * @return pointer to input buffer from where next header starts to. NULL in case of failure
 */
MQTTErrorCodes_t mqtt_connect(uint8_t * a_message_buffer_ptr, 
                              size_t a_max_buffer_size,
                              int * a_socket_desc_ptr,
                              MQTT_connect_t * a_connect_ptr,
                              bool wait_and_parse_response);
/**
 * Send MQTT disconnect
 *
 * Send out MQTT disconnect to given socket
 *
 * @param a_socket_desc_ptr [in] socket handler
 * @return error code @see MQTTErrorCodes_t 
 */
MQTTErrorCodes_t mqtt_disconnect(int * a_socket_desc_ptr);
