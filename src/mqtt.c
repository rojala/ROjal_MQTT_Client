#include "mqtt.h"
#include <string.h>    // memcpy

/****************************************************************************************
 * Local functions declaration                                                          *
 ****************************************************************************************/
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
 * @return pointer to input buffer from where next header starts to. NULL in case of failure
 */
uint8_t * decode_fixed_header(uint8_t * a_input_ptr,
                              bool * a_dup_ptr,
                              MQTTQoSLevel_t * a_qos_ptr,
                              bool * a_retain_ptr,
                              MQTTMessageType_t * a_message_type_ptr,
                              uint32_t * a_message_size_ptr);

/**
 * Construct variable header for connect message
 *
 * Fill in all fileds needed to build MQTT connect message.
 *
 * @param a_output_ptr [out] preallocated buffer where data is filled
 * @param a_clean_session [in] clean session bit
 * @param a_last_will_qos [in] QoS for last will @see MQTTQoSLevel_t
 * @param a_permanent_last_will [in] is last will permanent
 * @param a_password [in] payload contains password
 * @param a_username [in] payoad contains username
 * @param a_keepalive [in] 16bit keep alive counter
 * @return length of header = 10 bytes or 0 in case of failure
 */
uint8_t encode_variable_header_connect(uint8_t * a_output_ptr, 
                                       bool a_clean_session,
                                       bool a_last_will,
                                       MQTTQoSLevel_t a_last_will_qos,
                                       bool a_permanent_last_will,
                                       bool a_password,
                                       bool a_username,
                                       uint16_t a_keepalive);
/**
 * Decode connack from variable header stream.
 *
 * Parse out connection status from connak message.
 *
 * @param a_input_ptr [in] point to first byte of received MQTT message
 * @param a_connection_state_ptr [out] connection state. 0 = successfully connected
 * @return pointer to input buffer from where next header starts to. NULL in case of failure
 */
uint8_t * decode_variable_header_conack(uint8_t * a_input_ptr, uint8_t * a_connection_state_ptr);

/**
 * Set size into fixed header
 *
 * Construct size based on formula presented in MQTT protocol specification:  
 * @see http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.pdf chapter 2.2.3.
 *
 * @param a_output_ptr [out] pre-allocated puffer where size data will be written
 * @param a_message_size [in] remaining size of MQTT message
 * @return size of remaining length field in fixed header. 0 in case of failure.
 */
uint8_t set_size(MQTT_fixed_header_t * a_output_ptr, size_t a_message_size);

/**
 * Get size from received MQTT message
 *
 * Destruct size based on formula presented in MQTT protocol specification:  
 * @see http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.pdf chapter 2.2.3.
 *
 * @param a_input_ptr [in] pointer to start of received MQTT message
 * @param a_message_size_ptr [out] remaining size of MQTT message will be written to here
 * @return pointer to next header or NULL in case of failure
 */
uint8_t * get_size(uint8_t * a_input_ptr, uint32_t * a_message_size_ptr);


MQTTErrorCodes_t mqtt_ping_req(data_stream_out_fptr_t a_out_fptr);
MQTTErrorCodes_t mqtt_connect_parse_ack(uint8_t * a_message_in_ptr);

/****************************************************************************************
 * Test and debug functions                                                             *
 ****************************************************************************************/

/* Debug hex print function */
void hex_print(uint8_t * a_data_ptr, size_t a_size)
{
    for(size_t i = 0; i < a_size; i++)
        mqtt_printf("0x%02x ", a_data_ptr[i] & 0xff);
    mqtt_printf("\n");
}

/****************************************************************************************
 * Fixed Header functions                                                               *
 * Get and set remaining size                                                           *
 * @see http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.pdf chapter 2.2.3 *
 ****************************************************************************************/
uint8_t set_size(MQTT_fixed_header_t * a_output_ptr, size_t a_message_size)
{
    uint8_t return_value = 0;
    if ((MQTT_MAX_MESSAGE_SIZE > a_message_size) && /* Message size in boundaries 0-max */
        (NULL != a_output_ptr))                  /* Output pointer is not NULL       */
    {
        
        /* Construct size from MQTT message - see the spec.*/
        do {
            uint8_t encodedByte = a_message_size % 128;
            a_message_size = a_message_size / 128;
            if (a_message_size > 0)
                encodedByte = encodedByte | 128;
            a_output_ptr->length[return_value] = encodedByte;
            return_value++;
        } while (a_message_size > 0);
        
    } else {
        return 0;
    }
    return return_value;
}

uint8_t * get_size(uint8_t * a_input_ptr, uint32_t * a_message_size_ptr)
{
    uint32_t multiplier = 1;
    uint32_t value = 0;
    uint8_t  cnt = 1;
    uint8_t  aByte = 0;

    /* Verify input parameters */
    if ((NULL == a_input_ptr) &&
        (NULL == a_message_size_ptr))
    {
        mqtt_printf("%s %u Invalid parameters %p %p\n", 
                    __FILE__,
                    __LINE__,
                    a_input_ptr,
                    a_message_size_ptr);
        return NULL;
    }

    /* Destruct size from MQTT message - see the spec.*/
    *a_message_size_ptr = 0;
    do {
        aByte = a_input_ptr[cnt++];
        value += (aByte & 127) * multiplier;
        if (multiplier > (128*128*128)){
            printf("Message size is too big %s %i \n", __FILE__, __LINE__);
            return NULL;
        }
        multiplier *= 128;
    } while ((aByte & 128) != 0);

    /* Verify that size is supported by applicaiton */
    if (MQTT_MAX_MESSAGE_SIZE < value) {
        mqtt_printf("%s %u Size is too big %u\n", __FILE__, __LINE__, value); 
        return NULL;
    }

    *a_message_size_ptr = value;
    return (a_input_ptr + cnt);
}

/****************************************************************************************
 * Fixed Header functions                                                               *
 * Encode and decode fixed header functions                                             *
 ****************************************************************************************/
uint8_t encode_fixed_header(MQTT_fixed_header_t * a_output_ptr,
                            bool a_dup,
                            MQTTQoSLevel_t a_qos,
                            bool a_retain,
                            MQTTMessageType_t message_type,
                            uint32_t message_size)
{
    if (NULL != a_output_ptr)
    {
        /* Test QoS boundaires and store it */
        if (QoSInvalid > a_qos) {
            a_output_ptr->flagsAndType.qos = a_qos;
        } else {
            mqtt_printf("%s %u Invalid QoS %i\n", __FILE__, __LINE__, a_qos);
            return 0;
        }

        a_output_ptr->flagsAndType.dup = a_dup;

        a_output_ptr->flagsAndType.retain = a_retain;

        /* Test message type boundaires and store it */
        if (MAXCMD > message_type) {
            a_output_ptr->flagsAndType.message_type = message_type;
        } else {
            mqtt_printf("%s %u Invalid message type %u\n",
                        __FILE__,
                        __LINE__,
                        message_type);
            return 0;
        }

        /* Test message size boundaires and store it */
        uint8_t msgLenSize = set_size(a_output_ptr, message_size);
        if (msgLenSize > 0)
            return (set_size(a_output_ptr, message_size) + 1);
    }
    /* Return failure */
    return 0;
}

uint8_t * decode_fixed_header(uint8_t * a_input_ptr,
                              bool * a_dup_ptr,
                              MQTTQoSLevel_t * a_qos_ptr,
                              bool * a_retain_ptr,
                              MQTTMessageType_t * a_message_type_ptr,
                              uint32_t * a_message_size_ptr)
{
    uint8_t * return_ptr = NULL;
    
    /* Check input parameters against NULL pointers */
    if ((NULL != a_input_ptr) &&
        (NULL != a_dup_ptr) &&
        (NULL != a_qos_ptr) &&
        (NULL != a_message_type_ptr) &&
        (NULL != a_retain_ptr) &&
        (NULL != a_message_size_ptr))
    {
        *a_dup_ptr = false;
        *a_qos_ptr = 0;
        *a_message_type_ptr = 0;
        *a_message_size_ptr = -1;
        *a_retain_ptr = false;

        MQTT_fixed_header_t * input_header = (MQTT_fixed_header_t*)a_input_ptr;
        bool temp_dup = input_header->flagsAndType.dup;
        MQTTQoSLevel_t temp_qos = input_header->flagsAndType.qos;
        bool temp_retain = input_header->flagsAndType.retain;
        MQTTMessageType_t temp_message_type = input_header->flagsAndType.message_type;

        /* Destruct size from received message */
        return_ptr = get_size(a_input_ptr, a_message_size_ptr);

        /* Validate and store fixed header parameters */
        if ((NULL != return_ptr) &&               /* Size vas valid */ 
            (QoSInvalid != temp_qos) &&           /* QoS parameter is valid */
            ((INVALIDCMD <= temp_message_type) && /* Message type is in rage (allow 0) */
            (MAXCMD > temp_message_type))){
            /* Resut is valid, store parameters into function arguments */
            *a_dup_ptr = temp_dup;
            *a_qos_ptr = temp_qos;
            *a_retain_ptr = temp_retain;
            *a_message_type_ptr = temp_message_type;
        } else {
            mqtt_printf("%s %u Invalid argument %x %x %x %u %x %p\n",
                        __FILE__,
                        __LINE__,
                        temp_dup,
                        temp_qos,
                        temp_retain,
                        *a_message_size_ptr,
                        temp_message_type,
                        a_input_ptr);
        }
    } else {
        mqtt_printf("%s %u NULL argument given %p %p %p %p %p %p\n",
                    __FILE__,
                    __LINE__,
                    a_dup_ptr,
                    a_qos_ptr,
                    a_retain_ptr,
                    a_message_size_ptr,
                    a_message_type_ptr,
                    a_input_ptr);
    }
    return return_ptr;
}

uint8_t encode_variable_header_connect(uint8_t * a_output_ptr, 
                                       bool a_clean_session,
                                       bool a_last_will,
                                       MQTTQoSLevel_t a_last_will_qos,
                                       bool a_permanent_last_will,
                                       bool a_password,
                                       bool a_username,
                                       uint16_t a_keepalive)
{

    uint8_t variable_header_size = 0;
    if ((NULL != a_output_ptr) &&
        (QoSInvalid > a_last_will_qos))
    {
        MQTT_variable_header_connect_t * header_ptr = (MQTT_variable_header_connect_t*) a_output_ptr;
        header_ptr->length[0] = 0x00;
        header_ptr->length[1] = 0x04; /* length of protocol name which is const => length is const */
        header_ptr->procol_name[0] = 'M';
        header_ptr->procol_name[1] = 'Q';
        header_ptr->procol_name[2] = 'T';
        header_ptr->procol_name[3] = 'T';
        header_ptr->protocol_version = 0x04; /* 0x04 = 3.1.1 version */
        header_ptr->keepalive[0] = (a_keepalive >> 8) & 0xFF;
        header_ptr->keepalive[1] = (a_keepalive >> 0) & 0xFF;

        /* Fill in connection flags */
        MQTT_variable_header_connect_flags_t * header_flags_ptr = (MQTT_variable_header_connect_flags_t*)&(header_ptr->flags);
        header_flags_ptr->reserved = 0;
        header_flags_ptr->clean_session = a_clean_session;
        header_flags_ptr->last_will = a_last_will;
        header_flags_ptr->last_will_qos = a_last_will_qos;
        header_flags_ptr->permanent_will = a_permanent_last_will;
        header_flags_ptr->password = a_password;
        header_flags_ptr->username = a_username;
        variable_header_size = 10; /* fixed size */
    } else {
        mqtt_printf("%s %u Invalid argument given %p %x\n",
                    __FILE__,
                    __LINE__,
                    a_output_ptr,
                    a_last_will_qos);
    }
    return variable_header_size;
}

uint8_t * decode_variable_header_conack(uint8_t * a_input_ptr, uint8_t * a_connection_state_ptr)
{
    * a_connection_state_ptr = -1;
    if (NULL != a_input_ptr)
    {
        * a_connection_state_ptr = *(a_input_ptr + 1); /*2nd byte contains return value  */
    } else {
        mqtt_printf("%s %u NULL argument given %p\n",
                    __FILE__,
                    __LINE__,
                    a_input_ptr);
        return NULL;
    }
    return a_input_ptr + 2; /* CONNACK is always 2 bytes long. */
}

uint8_t * mqtt_add_payload_parameters(uint8_t * a_output_ptr, uint16_t a_length, uint8_t * a_parameter_ptr)
{
    /* Payload parameters conists of 2 byte length followed by parameter */
    * a_output_ptr = (a_length >> 8) & 0xFF;
    a_output_ptr++;
    * a_output_ptr = (a_length >> 0) & 0xFF;
    a_output_ptr++;
    memcpy(a_output_ptr, a_parameter_ptr, a_length);
    return a_output_ptr + a_length;
}

uint8_t * mqtt_connect_fill(uint8_t * a_message_buffer_ptr,
                            size_t a_max_buffer_size,
                            MQTT_connect_t * a_connect_ptr,
                            uint16_t * a_ouput_size_ptr)
{
    uint8_t sizeOfVarHdr, sizeOfFixedHdr = 0;

    /* Clear message buffer */
    memset(a_message_buffer_ptr, 0, sizeof(a_max_buffer_size));
    
    uint8_t * payload_ptr = a_message_buffer_ptr + sizeof(MQTT_fixed_header_t) + sizeof(MQTT_variable_header_connect_t);
    
    /* Fill client ID into payload. It must exists and it must be first parameter */
    if ((0 < strlen(a_connect_ptr->client_id)) &&
        (MQTT_CLIENT_ID_SIZE > strlen(a_connect_ptr->client_id))) {

        payload_ptr = mqtt_add_payload_parameters(payload_ptr, 
                                                  strlen(a_connect_ptr->client_id), 
                                                  a_connect_ptr->client_id);

    } else {
        * a_ouput_size_ptr = 0;
        return NULL;
    }

    /* Add Last Will topic and message to the payload, if present */
    if ((0 < strlen(a_connect_ptr->last_will_topic)) &&
        (MQTT_CONNECT_LAST_WILL_TOPIC_SIZE > strlen(a_connect_ptr->last_will_topic)) &&
        (0 < strlen(a_connect_ptr->last_will_message)) &&
        (MQTT_CONNECT_LAST_WILL_MESSAGE_SIZE > strlen(a_connect_ptr->last_will_message))) {

        a_connect_ptr->connect_flags.last_will = true;

        payload_ptr = mqtt_add_payload_parameters(payload_ptr,
                                                  strlen(a_connect_ptr->last_will_topic), 
                                                  a_connect_ptr->last_will_topic);

        payload_ptr = mqtt_add_payload_parameters(payload_ptr, 
                                                  strlen(a_connect_ptr->last_will_message), 
                                                  a_connect_ptr->last_will_message);
    } else {
        a_connect_ptr->connect_flags.last_will = false;
        a_connect_ptr->connect_flags.last_will_qos = QoS0;
    }

    /* Add username to the payload, if present */
    if ((0 < strlen(a_connect_ptr->username)) &&
        (MQTT_USERNAME_SIZE > strlen(a_connect_ptr->username))) {

        a_connect_ptr->connect_flags.username = true;

        payload_ptr = mqtt_add_payload_parameters(payload_ptr, 
                                                  strlen(a_connect_ptr->username), 
                                                  a_connect_ptr->username);
    } else {
        a_connect_ptr->connect_flags.username = false;
    }

    /* Add password to the payload, if present */
    if ((0 < strlen(a_connect_ptr->password)) &&
        (MQTT_PASSWORD_SIZE > strlen(a_connect_ptr->password))) {

        a_connect_ptr->connect_flags.password = true;

        payload_ptr = mqtt_add_payload_parameters(payload_ptr, 
                                                  strlen(a_connect_ptr->password), 
                                                  a_connect_ptr->password);
    } else {
        a_connect_ptr->connect_flags.password = false;
    }

    /* Construct variable header with given parameters */
    sizeOfVarHdr = encode_variable_header_connect(a_message_buffer_ptr + sizeof(MQTT_fixed_header_t),
                                                  a_connect_ptr->connect_flags.clean_session,
                                                  a_connect_ptr->connect_flags.last_will,
                                                  a_connect_ptr->connect_flags.last_will_qos,
                                                  a_connect_ptr->connect_flags.permanent_will,
                                                  a_connect_ptr->connect_flags.password,
                                                  a_connect_ptr->connect_flags.username,
                                                  a_connect_ptr->keepalive);

    uint32_t payloadSize = payload_ptr - (a_message_buffer_ptr + sizeof(MQTT_fixed_header_t) + sizeof(MQTT_variable_header_connect_t));

    MQTT_fixed_header_t temporaryFixedHeader;
    /* Form fixed header for CONNECT msg */
    sizeOfFixedHdr = encode_fixed_header(&temporaryFixedHeader,
                                         false,
                                         QoS0,
                                         false,
                                         CONNECT,
                                         payloadSize + sizeOfVarHdr);

    /* Count valid starting point for the message, because fixed header length variates between 2 to 4 bytes */
    uint8_t * message_ptr = a_message_buffer_ptr + (sizeof(MQTT_fixed_header_t) - sizeOfFixedHdr);

    /* Copy fixed header at the beginning of the message */
    memcpy((void*)message_ptr, (void*)&temporaryFixedHeader, sizeOfFixedHdr);

    * a_ouput_size_ptr = (payload_ptr - message_ptr);

    return message_ptr;
}

MQTTErrorCodes_t mqtt_connect_parse_ack(uint8_t * a_message_in_ptr)
{
    uint8_t connection_state = InvalidArgument;
    if (NULL != a_message_in_ptr) {
        bool dup, retain;
        MQTTQoSLevel_t qos;
        MQTTMessageType_t type;
        uint32_t size;
        // Decode fixed header
        uint8_t * nextHdr = decode_fixed_header(a_message_in_ptr, &dup, &qos, &retain, &type, &size);
        if ((NULL != nextHdr) &&
            (CONNACK == type)) {
            // Decode variable header
            decode_variable_header_conack(nextHdr, &connection_state);
        }
    }
    return connection_state;
}

MQTTErrorCodes_t mqtt_parse_ping_ack(uint8_t * a_message_in_ptr)
{
    if (NULL != a_message_in_ptr) {
        bool dup, retain;
        MQTTQoSLevel_t qos;
        MQTTMessageType_t type;
        uint32_t size;
        // Decode fixed header
        uint8_t * nextHdr = decode_fixed_header(a_message_in_ptr, &dup, &qos, &retain, &type, &size);
        if ((NULL != nextHdr) &&
            (PINGRESP == type)) {
            // Decode variable header
            return Successfull;
        }
    }
    return ServerUnavailabe;
}

/****************************************************************************************
 * MQTT CONNECT                                                                         *
 ****************************************************************************************/
MQTTErrorCodes_t mqtt_connect(uint8_t * a_message_buffer_ptr,
                              size_t a_max_buffer_size,
                              data_stream_in_fptr_t a_in_fptr,
                              data_stream_out_fptr_t a_out_fptr,
                              MQTT_connect_t * a_connect_ptr,
                              bool wait_and_parse_response)
{
    /* Ensure that pointers are valid */
    if ((NULL != a_in_fptr) &&
        (NULL != a_out_fptr)) {
        
        if ((NULL != a_message_buffer_ptr) &&
            (NULL != a_connect_ptr)) {
            uint16_t msg_size = 0;
            uint8_t * msg_ptr = mqtt_connect_fill(a_message_buffer_ptr,
                                                  a_max_buffer_size,
                                                  a_connect_ptr,
                                                  &msg_size);
            if (NULL != msg_ptr) {
                // Send CONNECT message to the broker without flags
                if (a_out_fptr(msg_ptr, msg_size) == msg_size)
                {
                    if (wait_and_parse_response)
                    {
                        // Wait response from borker
                        int rcv = a_in_fptr(a_message_buffer_ptr, sizeof(MQTT_fixed_header_t));
                        if (0 < rcv) {
                            return mqtt_connect_parse_ack(a_message_buffer_ptr);
                        } else
                        {
                            return ServerUnavailabe;
                        }
                    } else {
                        return Successfull;
                    }
                } else {
                    return ServerUnavailabe;
                }
            }   
        }
    }
    return InvalidArgument;
}

MQTTErrorCodes_t mqtt_disconnect(data_stream_out_fptr_t a_out_fptr)
{   
    if (NULL != a_out_fptr) {
        // Form and send fixed header with DISCONNECT command ID
        MQTT_fixed_header_t temporaryBuffer;
        uint8_t sizeOfFixedHdr = encode_fixed_header(&temporaryBuffer, false, QoS0, false, DISCONNECT, 0);
        if (a_out_fptr((uint8_t*)&temporaryBuffer, sizeOfFixedHdr) == sizeOfFixedHdr)
            return Successfull;
        else
            return ServerUnavailabe;
    }
    return InvalidArgument;
}

MQTTErrorCodes_t mqtt_ping_req(data_stream_out_fptr_t a_out_fptr)
{
    if (NULL != a_out_fptr) {
        // Form and send fixed header with PINGREQ command ID
        MQTT_fixed_header_t temporaryBuffer;
        uint8_t sizeOfFixedHdr = encode_fixed_header(&temporaryBuffer, false, QoS0, false, PINGREQ, 0);
        if (a_out_fptr((uint8_t*)&temporaryBuffer, sizeOfFixedHdr) == sizeOfFixedHdr)
            return Successfull;
        else
            return ServerUnavailabe;
    }
    return InvalidArgument;
}