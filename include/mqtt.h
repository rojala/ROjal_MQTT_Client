// FIXED HEADER TO OWN HDR FILE ?
// Keep mqtt.h which would include mqtt_fixed_header.h, variable_header.h and pyaload.h
#ifndef MQTT_H
#define MQTT_H
#include <stdint.h>  // uint
#include <stddef.h>  // size_t
#include <stdbool.h> // bool
#include <stdio.h>   // printf

#define MAX_SUPPORTED_SIZE (0x80000000 - 1)

typedef enum MQTTMessageType
{
    INVALIDCMD = 0,
    CONNECT = 1,
    CONNACT,
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
}MQTTMessageType_t;

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
    uint8_t retain:1;      /* Retain or not                   */
    uint8_t dup:1;         /* one bit value, duplicate or not */
    uint8_t qos:2;         /* Quality of service 0-2          */
    uint8_t message_type:4;
} struct_flags_and_type_t;

/* FIXED HEADER */
typedef struct MQTT_fixed_header
{
    struct_flags_and_type_t flagsAndType;
    uint8_t length[4];
} MQTT_fixed_header_t;

/**
 * Construct fixed header from given parameters.
 *
 * Fixed header flags, message type and size are set by
 * this function. Result is stored to pre-allocated
 * output buffer.
 *
 * @param output [out] is filled by the function (caller shall allocate and release)
 * @param dup [in] duplicate bit
 * @param qos [in] quality of service value @see MQTTQoSLevel_t
 * @param retain [in] retain bit
 * @param messageType [in] message type @see MQTTMessageType_t
 * @param msgSize [in] message folowed by the fixed header in bytes
 * @return size of header and 0 in case of failure
 */
uint8_t encode_fixed_header(MQTT_fixed_header_t * output,
                            bool dup,
                            MQTTQoSLevel_t qos,
                            bool retain,
                            MQTTMessageType_t messageType,
                            uint32_t msgSize);

/**
 * Decode fixed header from input stream.
 *
 * Fixed header flags, message type and size are set by
 * this function. Result is stored to pre-allocated
 * output buffer.
 *
 * @param a_input_ptr [in] point to first byte of received MQTT message
 * @param a_dup_ptr [out] duplicate bit
 * @param a_qos_ptr [out] quality of service value @see MQTTQoSLevel_t
 * @param a_retain_ptr [out] retain bit
 * @param a_message_type_ptr [out] message type @see MQTTMessageType_t
 * @param a_message_size_ptr [out] message folowed by the fixed header in bytes
 * @return pointer to input buffer from where next header starts to. NULL in case of failure.
 */
uint8_t * decode_fixed_header(uint8_t * a_input_ptr,
                              bool * a_dup_ptr,
                              MQTTQoSLevel_t * a_qos_ptr,
                              bool * a_retain_ptr,
                              MQTTMessageType_t * a_message_type_ptr,
                              uint32_t * a_message_size_ptr);

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
