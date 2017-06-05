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

typedef enum MQTTDup
{
    NoDUP = 0,
    DUP,
    DUPInvalid
}MQTTDup_t;

typedef struct struct_flags_and_type
{
    uint8_t reserved:1;    /* Unused - set to zero            */
    uint8_t dup:1;         /* one bit value, duplicate or not */
    uint8_t qos:2;         /* Quality of service 0-2          */
    uint8_t message_type:4;
} struct_flags_and_type_t;

/* FIXED HEADER */
#pragma pack(1)
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
 * @param dup [in] duplicate bit @see MQTTDup_t
 * @param qos [in] quality of service value @see MQTTQoSLevel_t
 * @param messageType [in] message type @see MQTTMessageType_t
 * @param msgSize [in] message folowed by the fixed header in bytes
 * @return size of header and 0 in case of failure
 */
uint8_t encode_fixed_header(MQTT_fixed_header_t * output,
                            MQTTDup_t dup,
                            MQTTQoSLevel_t qos,
                            MQTTMessageType_t messageType,
                            uint32_t msgSize);

/**
 * Decode fixed header from input stream.
 *
 * Fixed header flags, message type and size are set by
 * this function. Result is stored to pre-allocated
 * output buffer.
 *
 * @param input [in] point to first byte of received MQTT message
 * @param dup [out] duplicate bit @see MQTTDup_t
 * @param qos [out] quality of service value @see MQTTQoSLevel_t
 * @param messageType [out] message type @see MQTTMessageType_t
 * @param msgSize [out] message folowed by the fixed header in bytes
 * @return pointer to input buffer from where next header starts to. NULL in case of failure.
 */
uint8_t * decode_fixed_header(uint8_t * input,
                              MQTTDup_t * dup,
                              MQTTQoSLevel_t * qos,
                              MQTTMessageType_t * messageType,
                              uint32_t * msgSize);

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
