#include "mqtt.h"

/****************************************************************************************
 * Local functions declaration                                                          *
 ****************************************************************************************/
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
    if ((MAX_SUPPORTED_SIZE > a_message_size) && /* Message size in boundaries 0-max */
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
    if (MAX_SUPPORTED_SIZE < value) {
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
/****************************************************************************************
 * Fixed Header functions                                                               *
 * Encode and decode fixed header functions                                             *
 ****************************************************************************************/
