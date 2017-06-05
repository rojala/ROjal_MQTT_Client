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
    for(size_t i = 0; i < a_size; i++){
        mqtt_printf("0x%02x ", a_data_ptr[i] & 0xff);
    }
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
uint8_t encode_fixed_header(MQTT_fixed_header_t * output,
                            MQTTDup_t dup,
                            MQTTQoSLevel_t qos,
                            MQTTMessageType_t message_type,
                            uint32_t message_size)
{
    if (NULL != output)
    {
        /* Test QoS boundaires and store it */
        if (QoSInvalid > qos) {
            output->flagsAndType.qos = qos;
        } else {
            mqtt_printf("%s %u Invalid QoS\n", __FILE__, __LINE__);
            return 0;
        }

        /* Test DUP boundaires and store it */
        if (DUPInvalid > dup) {
            output->flagsAndType.dup = dup;
        } else {
            mqtt_printf("%s %u Invalid DUP\n", __FILE__, __LINE__);
            return 0;
        }

        output->flagsAndType.reserved = 0;

        /* Test message type boundaires and store it */
        if (MAXCMD > message_type) {
            output->flagsAndType.message_type = message_type;
        } else {
            mqtt_printf("%s %u Invalid message type %u\n",
                        __FILE__,
                        __LINE__,
                        message_type);
            return 0;
        }

        /* Test message size boundaires and store it */
        uint8_t msgLenSize = set_size(output, message_size);
        if (msgLenSize > 0)
            return (set_size(output, message_size) + 1);
    }
    /* Return failure */
    return 0;
}

uint8_t * decode_fixed_header(uint8_t * a_input_ptr,
                              MQTTDup_t * a_dup_ptr,
                              MQTTQoSLevel_t * a_qos_ptr,
                              MQTTMessageType_t * a_message_type_ptr,
                              uint32_t * a_message_size_ptr)
{
    uint8_t * return_ptr = NULL;
    
    /* Check input parameters against NULL pointers */
    if ((NULL != a_input_ptr) &&
        (NULL != a_dup_ptr) &&
        (NULL != a_qos_ptr) &&
        (NULL != a_message_type_ptr) &&
        (NULL != a_message_size_ptr))
    {
        *a_dup_ptr = 0;
        *a_qos_ptr = 0;
        *a_message_type_ptr = 0;
        *a_message_size_ptr = -1;

        MQTT_fixed_header_t * input_header = (MQTT_fixed_header_t*)a_input_ptr;
        MQTTDup_t temp_dup = input_header->flagsAndType.dup;
        MQTTQoSLevel_t temp_qos = input_header->flagsAndType.qos;
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
            *a_message_type_ptr = temp_message_type;
        } else {
            mqtt_printf("%s %u Invalid argument %x %x %u %x %p\n",
                        __FILE__,
                        __LINE__,
                        temp_dup,
                        temp_qos,
                        *a_message_size_ptr,
                        temp_message_type,
                        a_input_ptr);
        }
    } else {
        mqtt_printf("%s %u NULL argument given %p %p %p %p %p\n",
                    __FILE__,
                    __LINE__,
                    a_dup_ptr,
                    a_qos_ptr,
                    a_message_size_ptr,
                    a_message_type_ptr,
                    a_input_ptr);
    }
    return return_ptr;
}

/****************************************************************************************
 * Fixed Header functions                                                               *
 * Encode and decode fixed header functions                                             *
 ****************************************************************************************/
