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

#include "mqtt.h"
#include <string.h>    // memcpy

static MQTT_shared_data_t * g_shared_data = NULL;

/************************************************************************************************************
 *                                                                                                          *
 * Declaration of local functions                                                                           *
 *                                                                                                          *
 ************************************************************************************************************/
 
 /**
 * Decode connack from variable header stream.
 *
 * Parse out connection status from connak message.
 *
 * @param a_message_buffer_ptr [out] allocated working space
 * @param a_max_buffer_size [in] maximum size of the working space
 * @param a_in_fptr [in] input stream callback function (receive)
 * @param a_out_fptr [in] output stream callback function (send)
 * @param a_connect_ptr [in] connection parameters @see MQTT_connect_t
 * @param wait_and_parse_response [in] when true, function will wait connak response from the broker and parse it
 * @return pointer to input buffer from where next header starts to. NULL in case of failure
 */
MQTTErrorCodes_t mqtt_connect(uint8_t                * a_message_buffer_ptr, 
                              size_t                   a_max_buffer_size,
                              data_stream_in_fptr_t    a_in_fptr,
                              data_stream_out_fptr_t   a_out_fptr,
                              MQTT_connect_t         * a_connect_ptr,
                              bool                     wait_and_parse_response);

/**
 * Send MQTT disconnect
 *
 * Send out MQTT disconnect to given socket
 *
 * @param a_out_fptr [in] output stream callback function
 * @return error code @see MQTTErrorCodes_t 
 */
MQTTErrorCodes_t mqtt_disconnect(data_stream_out_fptr_t a_out_fptr);

/**
 * Send PING request
 *
 * Construct ping request by building fixed header and send data out by calling
 * given funciton pointer.
 *
 * @param a_out_fptr [in] output stream callback function
 * @return error code @see MQTTErrorCodes_t 
 */
MQTTErrorCodes_t mqtt_ping_req(data_stream_out_fptr_t a_out_fptr);

/**
 * Validate connection response
 *
 * Connect ack message is validated by this funciton.
 *
 * @param a_message_in_ptr [in] start of MQTT message
 * @return error code @see MQTTErrorCodes_t 
 */
MQTTErrorCodes_t mqtt_connect_parse_ack(uint8_t * a_message_in_ptr);
										 
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
uint8_t set_size(MQTT_fixed_header_t * a_output_ptr,
                 size_t                a_message_size);

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
uint8_t * get_size(uint8_t  * a_input_ptr, 
                   uint32_t * a_message_size_ptr);


/************************************************************************************************************
 *                                                                                                          *
 * Declaration of local decode functions                                                                    *
 *                                                                                                          *
 ************************************************************************************************************/

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
uint8_t * decode_fixed_header(uint8_t           * a_input_ptr,
                              bool              * a_dup_ptr,
                              MQTTQoSLevel_t    * a_qos_ptr,
                              bool              * a_retain_ptr,
                              MQTTMessageType_t * a_message_type_ptr,
                              uint32_t          * a_message_size_ptr);

/**
 * Decode connack from variable header stream.
 *
 * Parse out connection status from connak message.
 *
 * @param a_input_ptr [in] point to first byte of received MQTT message.
 * @param a_connection_state_ptr [out] connection state. 0 = successfully connected.
 * @return pointer to input buffer from where next header starts to. NULL in case of failure.
 */
uint8_t * decode_variable_header_conack(uint8_t * a_input_ptr, 
                                        uint8_t * a_connection_state_ptr);

/**
 * Decode variable header suback frame.
 *
 * Decode variable header suback frame which is received after subscribe command has been sent to 
 * broker. Variable header contains subscribe status information for the subscribe request.
 *
 * @param a_input_ptr [in] point to first byte of variable header.
 * @param a_subscribe_state_ptr [out] Result of subscribe will be stored into this variable location.
 */
void decode_variable_header_suback(uint8_t          * a_input_ptr, 
                                   MQTTErrorCodes_t * a_subscribe_state_ptr);

/**
 * Decode variable header publish frame.
 *
 * Decode variable header suback frame from given input stream. Varialbe header contains
 * topic name, length and topic quality level, which are parsed out from the given byte stream.
 *
 * @param a_input_ptr [in] point to first byte of variable header.
 * @param a_topic_out_ptr [out] this will point to location from where topic start on input stream.
 * @param a_qos [out] not used at this point, would contain QoS information of published message.
 * @param a_topic_length [out] topic length is written to to this parameter.
 * @return pointer to input buffer from where payload starts. NULL in case of failure.
 */
uint8_t * decode_variable_header_publish(uint8_t        *  a_input_ptr, 
										 uint8_t        ** a_topic_out_ptr,
										 MQTTQoSLevel_t    a_qos,
										 uint8_t        *  a_topic_length);

/**
 * Decode complete publish message
 *
 * Decode publish message using fixed header and variable header functions. 
 *
 * @param a_message_in_ptr [in] pointer to variable header part of a MQTT publish message
 * @param a_size_of_msg [in] size of message.
 * @param a_qos [out] not used at this point, would contain QoS information of published message.
 * @param a_topic_out_ptr [out] will point to beginning of topic in given input stream.
 * @param a_topic_length_out_ptr [out] topic length is written to to this parameter.
 * @param a_out_message_ptr [out] will point to beginning of payload in given input stream.
 * @param a_out_message_size_ptr [out] payload length is written to to this parameter.
 * @return pointer to input buffer from where payload starts. NULL in case of failure.
 */			 
bool decode_publish(uint8_t        *  a_message_in_ptr,
					uint32_t          a_size_of_msg,
                    MQTTQoSLevel_t    a_qos,
					uint8_t        ** a_topic_out_ptr,
					uint8_t        *  a_topic_length_out_ptr,
					uint8_t        ** a_out_message_ptr,
					uint8_t        *  a_out_message_size_ptr);


/************************************************************************************************************
 *                                                                                                          *
 * Declaration for internal encode functions                                                                *
 *                                                                                                          *
 ************************************************************************************************************/
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
uint8_t encode_variable_header_connect(uint8_t        * a_output_ptr, 
                                       bool             a_clean_session,
                                       bool             a_last_will,
                                       MQTTQoSLevel_t   a_last_will_qos,
                                       bool             a_permanent_last_will,
                                       bool             a_password,
                                       bool             a_username,
                                       uint16_t         a_keepalive);
	   
/**
 * Decode variable header publish frame.
 *
 * Decode variable header suback frame from given input stream. Varialbe header contains
 * topic name, length and topic quality level, which are parsed out from the given byte stream.
 *
 * @param a_input_ptr [in] point to first byte of variable header
 * @param a_topic_out_ptr [out] this will point to location from where topic start on input stream.
 * @param a_qos [out] not used at this point, would contain QoS information of published message.
 * @param a_topic_length [out] topic length is written to to this parameter.
 * @return pointer to input buffer from where payload starts. NULL in case of failure.
 */
bool encode_publish(data_stream_out_fptr_t   a_out_fptr,
                    uint8_t                * a_output_ptr,
                    uint32_t                 a_output_size,
                    bool                     a_retain,
                    MQTTQoSLevel_t           a_qos,
                    bool                     a_dup,
                    uint8_t                * topic_ptr,
                    uint16_t                 topic_size,
                    uint16_t                 packet_identifier,
                    uint8_t                * message_ptr,
                    uint32_t                 message_size);

 /**
 * Construct fixed header from given parameters.
 *
 * Fixed header flags, message type and size are set by
 * this function. Result is stored to pre-allocated
 * output buffer.
 *
 * @param a_output_ptr [out] is filled by the function (caller shall allocate and release)
 * @param a_dup [in] duplicate bit
 * @param a_qos [in] quality of service value @see MQTTQoSLevel_t
 * @param a_retain [in] retain bit
 * @param a_messageType [in] message type @see MQTTMessageType_t
 * @param a_msgSize [in] message folowed by the fixed header in bytes
 * @return size of header and 0 in case of failure
 */
uint8_t encode_fixed_header(MQTT_fixed_header_t * a_output_ptr,
                            bool                  a_dup,
                            MQTTQoSLevel_t        a_qos,
                            bool                  a_retain,
                            MQTTMessageType_t     a_messageType,
                            uint32_t              a_msgSize);

 /**
 * Construct fixed header from given parameters.
 *
 * Fixed header flags, message type and size are set by
 * this function. Result is stored to pre-allocated
 * output buffer.
 *
 * @param a_out_fptr [in] function pointer, which is called to send message out.
 * @param a_output_ptr [out] ouptut buffer, where date is stored before sending (caller ensure validity).
 * @param a_output_size [in] maximum size of given output buffer.
 * @param a_topic_qos [in] QoS for the topic.
 * @param a_topic_ptr [in] poitner to topic. 
 * @param a_topic_size [in] size of the topic.
 * @param a_packet_identifier [in] packet sequence number.
 * @return true or false
 */
bool encode_subscribe(data_stream_out_fptr_t   a_out_fptr,
                      uint8_t                * a_output_ptr,
                      uint32_t                 a_output_size,
                      MQTTQoSLevel_t           a_topic_qos,
                      uint8_t                * a_topic_ptr,
                      uint16_t                 a_topic_size,
                      uint16_t                 a_packet_identifier);


/************************************************************************************************************
 * Test and debug functions                                                                                 *
 ************************************************************************************************************/

/* Debug hex print function */
void hex_print(uint8_t * a_data_ptr, size_t a_size)
{
    for (size_t i = 0; i < a_size; i++)
        mqtt_printf("0x%02x ", a_data_ptr[i] & 0xff);
    mqtt_printf("\n");
}

/************************************************************************************************************
 * Fixed Header functions                                                                                   *
 * Get message size and set message size into fixed header                                                  *
 * @see http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.pdf chapter 2.2.3                     *
 ************************************************************************************************************/
uint8_t set_size(MQTT_fixed_header_t * a_output_ptr,
                 size_t a_message_size)
{
    uint8_t return_value = 0;
    if ((MQTT_MAX_MESSAGE_SIZE > a_message_size) && /* Message size in boundaries 0-max */
        (NULL != a_output_ptr))                     /* Output pointer is not NULL       */
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

uint8_t * get_size(uint8_t * a_input_ptr,
                   uint32_t * a_message_size_ptr)
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
            mqtt_printf("Message size is too big %s %i \n", __FILE__, __LINE__);
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

/****************************************************************************************
 * Variable header functions                                                            *
 * Encode and decode fixed header functions                                             *
 ****************************************************************************************/
 
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

bool encode_publish(data_stream_out_fptr_t a_out_fptr,
                    uint8_t * a_output_ptr,
                    uint32_t a_output_size,
                    bool a_retain,
                    MQTTQoSLevel_t a_qos,
                    bool a_dup,
                    uint8_t * topic_ptr,
                    uint16_t topic_size,
                    uint16_t packet_identifier,
                    uint8_t * message_ptr,
                    uint32_t message_size)
{
    bool ret = false;

    if ((NULL != a_output_ptr) &&
        (NULL != topic_ptr)) {

        uint32_t sizeOfMsg = message_size + topic_size + sizeof(uint16_t);

        if (a_qos > QoS0)
            sizeOfMsg += sizeof(uint16_t);

        sizeOfMsg = encode_fixed_header((MQTT_fixed_header_t *) a_output_ptr,
                                                a_retain,
                                                a_qos,
                                                a_dup,
                                                PUBLISH,
                                                sizeOfMsg);

        
        hex_print((uint8_t *) a_output_ptr, sizeOfMsg);


        if (0 < sizeOfMsg) {
            /* First 2 bytes are topic_size */
            a_output_ptr[sizeOfMsg++] = ((topic_size >> 8) & 0xFF);
            a_output_ptr[sizeOfMsg++] = ((topic_size >> 0) & 0xFF);

            /* Copy topic name */
            memcpy((void*)&(a_output_ptr[sizeOfMsg]), topic_ptr, topic_size);
            sizeOfMsg += topic_size;

            if (a_qos > QoS0) {
                /* Copy packet identifier - valid only in QoS 1 and 2 levels */
                a_output_ptr[sizeOfMsg++] = ((packet_identifier >> 8) & 0xFF);
                a_output_ptr[sizeOfMsg++] = ((packet_identifier >> 0) & 0xFF);
            }

            /* Copy message of topic */
            memcpy((void*)&(a_output_ptr[sizeOfMsg]), message_ptr, message_size);
            sizeOfMsg +=message_size;

            // Send CONNECT message to the broker without flags
            if (a_out_fptr(a_output_ptr, sizeOfMsg) == sizeOfMsg)
                ret = true;
            else
                mqtt_printf("%s %u Sending publish failed %u",
                            __FILE__,
                            __LINE__,
                            sizeOfMsg);
        } else {
            mqtt_printf("%s %u Fixed header failed ",
                        __FILE__,
                        __LINE__);
        }
    } else {
        mqtt_printf("%s %u Invalid argument given %p %s\n",
                    __FILE__,
                    __LINE__,
                    a_output_ptr,
                    topic_ptr);
    }
    return ret;
}


bool decode_publish(uint8_t * a_message_in_ptr,
					uint32_t a_size_of_msg,
                    MQTTQoSLevel_t a_qos,
					uint8_t ** a_topic_out_ptr,
					uint8_t * a_topic_length_out_ptr,
					uint8_t ** a_out_message_ptr,
					uint8_t * a_out_message_size_ptr)
{
    bool ret = false;

	if (NULL != a_message_in_ptr) {
	
		/* Decode variable header = topic name and length - read topic out and get pointer to payload */
		uint8_t * payload = decode_variable_header_publish(a_message_in_ptr,
		                                                   a_topic_out_ptr, 
														   a_qos,
														   a_topic_length_out_ptr);
							   
		if ((NULL != payload) &&
		     (NULL != a_topic_out_ptr) &&
			 (NULL != a_out_message_size_ptr) &&
		     (0 < (*a_topic_length_out_ptr))) {
			
			uint32_t header_size = (payload - a_message_in_ptr);
			
			if ( header_size < a_size_of_msg ) {
				*a_out_message_size_ptr = a_size_of_msg - header_size;
				
				*a_out_message_ptr = payload;

				ret = true;
			} else {
				mqtt_printf("%s %u header size is bigger than reserved message size %u > %u\n",
							__FILE__,
							__LINE__,
							header_size,
							a_size_of_msg);
			}
		} else {
			mqtt_printf("%s %u variable decode failed\n",
						__FILE__,
						__LINE__);
		}

    } else {
        mqtt_printf("%s %u Invalid argument given %p\n",
                    __FILE__,
                    __LINE__,
                    a_message_in_ptr);
    }
    return ret;
}

bool encode_subscribe(data_stream_out_fptr_t a_out_fptr,
                      uint8_t * a_output_ptr,
                      uint32_t a_output_size,
                      MQTTQoSLevel_t a_topic_qos,
                      uint8_t * a_topic_ptr,
                      uint16_t a_topic_size,
                      uint16_t a_packet_identifier)
{
    bool ret = false;

    if ((NULL != a_output_ptr) &&
        (NULL != a_topic_ptr)) {

		/* topic string size + topic QoS + topic length + packet identifier */
        uint32_t sizeOfMsg =  a_topic_size + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t);

        if (a_topic_qos > QoS0)
            sizeOfMsg += sizeof(uint16_t);

		/* In subscribe QoS must be 1 and rest remain zero */
        sizeOfMsg = encode_fixed_header((MQTT_fixed_header_t *) a_output_ptr,
                                                true,
                                                QoS0,
                                                false,
                                                SUBSCRIBE,
                                                sizeOfMsg);

        hex_print((uint8_t *) a_output_ptr, sizeOfMsg);

        if (0 < sizeOfMsg) {

			hex_print((uint8_t *) a_output_ptr, sizeOfMsg);

            if (1 /*a_topic_qos > QoS0*/) {
                /* Copy packet identifier - valid only in QoS 1 and 2 levels */
                a_output_ptr[sizeOfMsg++] = ((a_packet_identifier >> 8) & 0xFF);
                a_output_ptr[sizeOfMsg++] = ((a_packet_identifier >> 0) & 0xFF);
            }

			hex_print((uint8_t *) a_output_ptr, sizeOfMsg);

			/* Copy topic length */
			a_output_ptr[sizeOfMsg++] = ((a_topic_size >> 8) & 0xFF);
			a_output_ptr[sizeOfMsg++] = ((a_topic_size >> 0) & 0xFF);
			
            /* Copy topic name */
            memcpy((void*)&(a_output_ptr[sizeOfMsg]), a_topic_ptr, a_topic_size);
            sizeOfMsg += a_topic_size;

            /* QoS for subscribe */
            a_output_ptr[sizeOfMsg++] = a_topic_qos;

            hex_print((uint8_t *) a_output_ptr, sizeOfMsg);

            /* Send SUBSCRIBE message */
            if (a_out_fptr(a_output_ptr, sizeOfMsg) == sizeOfMsg)
                ret = true;
            else
                mqtt_printf("%s %u Sending SUBSCRIBE failed %u,\n",
                            __FILE__,
                            __LINE__,
                            sizeOfMsg);
        } else {
            mqtt_printf("%s %u Fixed header failed\n",
                        __FILE__,
                        __LINE__);
        }
    } else {
        mqtt_printf("%s %u Invalid argument given %p %s\n",
                    __FILE__,
                    __LINE__,
                    a_output_ptr,
                    a_topic_ptr);
    }
    return ret;
}

uint8_t * decode_variable_header_conack(uint8_t * a_input_ptr, uint8_t * a_connection_state_ptr)
{
    * a_connection_state_ptr = -1;
    if (NULL != a_input_ptr)
    {
        * a_connection_state_ptr = *(a_input_ptr + 1); /*2nd byte contains return value  */
        mqtt_printf("%s %u CONNACK %x\n", __FILE__, __LINE__, * a_connection_state_ptr);
    } else {
        mqtt_printf("%s %u NULL argument given %p\n",
                    __FILE__,
                    __LINE__,
                    a_input_ptr);
        return NULL;
    }
    return a_input_ptr + 2; /* CONNACK is always 2 bytes long. */
}

void decode_variable_header_suback(uint8_t * a_input_ptr, MQTTErrorCodes_t * a_subscribe_state_ptr)
{
    * a_subscribe_state_ptr = -1;
    if (NULL != a_input_ptr)
    {
        * a_subscribe_state_ptr = *(a_input_ptr + 4); /*5th byte contains return value  */
        mqtt_printf("%s %u SUBACK %x\n", __FILE__, __LINE__, * a_subscribe_state_ptr);
    } else {
        mqtt_printf("%s %u NULL argument given %p\n",
                    __FILE__,
                    __LINE__,
                    a_input_ptr);
    }
	
	* a_subscribe_state_ptr = (* a_subscribe_state_ptr == 0x80); /* 0x80 is failure and 0 status is successfull. TODO - 0-3 are successfull QoS levels*/
}

uint8_t * decode_variable_header_publish(uint8_t * a_input_ptr, 
									     uint8_t ** a_topic_out_ptr,
										 MQTTQoSLevel_t a_qos, 
										 uint8_t * a_topic_length_out_ptr)
{
	uint8_t * next_hdr = NULL;
	
    if ((NULL != a_input_ptr) &&
	    (NULL != a_topic_length_out_ptr)) {

		uint32_t index = 0;
		/* First 2 bytes are topic_size */
		*a_topic_length_out_ptr  = ((a_input_ptr[index++] >> 8) & 0xFF);
		*a_topic_length_out_ptr |= ((a_input_ptr[index++] >> 0) & 0xFF);
		
		/* Set pointer to point beginning of topic - no copy, reuse existing buffer. */
		*a_topic_out_ptr = &(a_input_ptr[index++]);
		
		index += *a_topic_length_out_ptr;

		if (a_qos > QoS0)
			/* Ignore 2 bytes packet identifiers - valid only in QoS 1 and 2 levels */
			index += 2;

		next_hdr = (uint8_t*)&(a_input_ptr[index-1]);
    } else {
        mqtt_printf("%s %u NULL argument given %p\n",
                    __FILE__,
                    __LINE__,
                    a_input_ptr);
        return NULL;
    }
    return next_hdr;
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
    if (NULL != a_out_fptr) {
        
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
                    if ((NULL != a_in_fptr) &&
                        (true == wait_and_parse_response))
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


MQTTErrorCodes_t mqtt_parse_input_stream(uint8_t * a_input_ptr, 
                                         uint32_t * a_message_size_ptr)
{
    MQTTErrorCodes_t status = InvalidArgument;
    bool dup, retain;
    MQTTQoSLevel_t qos;
    MQTTMessageType_t type;
    uint8_t * next_header_ptr = decode_fixed_header(a_input_ptr, &dup, &qos, &retain, &type, a_message_size_ptr);
    mqtt_printf("%s %u mqtt_parse_input_stream %i %p %x\n", __FILE__, __LINE__, type, a_input_ptr, *a_message_size_ptr);
    switch (type)
    {
        case CONNACK:
			{
				uint8_t connection_state;
				decode_variable_header_conack(next_header_ptr, &connection_state);
				mqtt_printf("Conn ack %i\n", connection_state);
				if (Successfull == connection_state) {
					g_shared_data->state = STATE_CONNECTED;
					status = Successfull;
					mqtt_printf("connected\n");
				} else {
					g_shared_data->state = STATE_DISCONNECTED;
					status = Successfull;
					mqtt_printf("Disconnected\n");
				}
				if (NULL != g_shared_data->connected_cb_fptr)
					g_shared_data->connected_cb_fptr(connection_state);
				else
					mqtt_printf("Connection callback is NULL\n");
			}
            break;
        case PUBLISH:
			{
				uint8_t * topic_ptr    = NULL;
				uint16_t  topic_length = 0;
				uint8_t * message_ptr  = NULL;
				uint32_t  message_size = 0;

				if (decode_publish(next_header_ptr,
								   * a_message_size_ptr,
								   qos,
								   &topic_ptr,
								   (uint8_t *) &topic_length,
								   &message_ptr,
								   (uint8_t *) &message_size)){

					if (NULL != g_shared_data->subscribe_cb_fptr)
						g_shared_data->subscribe_cb_fptr(Successfull, 
														 message_ptr,
														 message_size,
														 topic_ptr,
														 topic_length);
					else
						mqtt_printf("Subscribe callback is not set\n");
					status = Successfull; 
				} else {
					if (NULL != g_shared_data->subscribe_cb_fptr)
						g_shared_data->subscribe_cb_fptr(status, NULL, 0, NULL, 0);
				}
				break;
			}
        case SUBACK:
			{
				decode_variable_header_suback(a_input_ptr, &status);
				if (NULL != g_shared_data->subscribe_cb_fptr)
					g_shared_data->subscribe_cb_fptr(Successfull, NULL, 0, NULL, 0);
				else
					g_shared_data->subscribe_cb_fptr(PublishDecodeError, NULL, 0, NULL, 0);
				status = Successfull;
			}
            break;
        case UNSUBACK:
            /* not implemented */
            break;
        case PINGRESP:
            status = Successfull;
            break;
        default:
            status = InvalidArgument;
            break;
    }
    return status;
}

/****************************************************************************************
 * MQTT State Maschine                                                                  *
 ****************************************************************************************/

 #include <time.h>
void timestamp()
{
    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */
    printf("%s",asctime( localtime(&ltime) ) );
}

MQTTErrorCodes_t mqtt(MQTTAction_t a_action,
                      MQTT_action_data_t * a_action_ptr)
{
        MQTTErrorCodes_t status = InvalidArgument;
		
		mqtt_printf("Action %i\n", a_action);

        switch (a_action)
        {
            case ACTION_INIT:
                if (NULL != a_action_ptr) {
                    g_shared_data = a_action_ptr->action_argument.shared_ptr;
                    g_shared_data->state = STATE_DISCONNECTED;
                    g_shared_data->mqtt_time = 0;
                    g_shared_data->mqtt_packet_cntr = 0;
                    g_shared_data->keepalive_in_ms = 0;
                    g_shared_data->last_updated_timer_in_ms = 0;
                    status = Successfull;
                }
                break;

            case ACTION_DISCONNECT:
                if ((g_shared_data != NULL) &&
                    (g_shared_data->state != STATE_DISCONNECTED))
                    status = mqtt_disconnect(g_shared_data->out_fptr);
                else
                    status = NoConnection;
                break;

            case ACTION_CONNECT:
                if ((NULL != g_shared_data) &&
                    (NULL != a_action_ptr)) {
                    if (g_shared_data->state == STATE_DISCONNECTED) {
                        status = mqtt_connect(g_shared_data->buffer, 
                                              g_shared_data->buffer_size,
                                              NULL,
                                              g_shared_data->out_fptr,
                                              a_action_ptr->action_argument.connect_ptr,
                                              false);

                        if (Successfull == status) {
                            if (0 != a_action_ptr->action_argument.connect_ptr->keepalive) {
                                g_shared_data->keepalive_in_ms = (a_action_ptr->action_argument.connect_ptr->keepalive) * 1000;
                                g_shared_data->keepalive_in_ms -= 500;
                            } else {
                                g_shared_data->keepalive_in_ms = INT32_MIN;
                            }
                            g_shared_data->last_updated_timer_in_ms = 0; /*g_shared_data->keepalive_in_ms;*/
                            g_shared_data->state = STATE_CONNECTED;
                        } else {
                            g_shared_data->state = STATE_DISCONNECTED;
                        }
                    } else {
                        status = AllreadyConnected;
                    }
                }
                break;

            case ACTION_PUBLISH:
                if ((NULL != g_shared_data) &&
                    (g_shared_data->state == STATE_CONNECTED) &&
                    (NULL != a_action_ptr)) {
					hex_print((uint8_t *) a_action_ptr->action_argument.publish_ptr->message_buffer_ptr, a_action_ptr->action_argument.publish_ptr->message_buffer_size);
            
                        if (true == encode_publish(g_shared_data->out_fptr,
                                                   g_shared_data->buffer, 
                                                   g_shared_data->buffer_size,
                                                   a_action_ptr->action_argument.publish_ptr->flags.retain,
                                                   a_action_ptr->action_argument.publish_ptr->flags.qos,
                                                   false, /* a_action_ptr->action_argument.publish_ptr->flags.dup,*/
                                                   a_action_ptr->action_argument.publish_ptr->topic_ptr,
                                                   a_action_ptr->action_argument.publish_ptr->topic_length,
                                                   g_shared_data->mqtt_packet_cntr++,
                                                   a_action_ptr->action_argument.publish_ptr->message_buffer_ptr,
                                                   a_action_ptr->action_argument.publish_ptr->message_buffer_size)) {
                       
                            g_shared_data->last_updated_timer_in_ms = g_shared_data->keepalive_in_ms;
                            status = Successfull;
                        }
                }
                break;

            case ACTION_SUBSCRIBE:

                if ((NULL != g_shared_data) &&
                    (g_shared_data->state == STATE_CONNECTED) &&
                    (NULL != a_action_ptr)) {

                        if (true == encode_subscribe(g_shared_data->out_fptr,
                                                     g_shared_data->buffer, 
                                                     g_shared_data->buffer_size,
                                                     a_action_ptr->action_argument.subscribe_ptr->qos,
                                                     a_action_ptr->action_argument.subscribe_ptr->topic_ptr,
                                                     a_action_ptr->action_argument.subscribe_ptr->topic_length,
                                                     g_shared_data->mqtt_packet_cntr++)) {
                            g_shared_data->last_updated_timer_in_ms = g_shared_data->keepalive_in_ms;
                            status = Successfull;
                        }
                }
                break;

            case ACTION_KEEPALIVE:
                if (NULL != a_action_ptr) {
                    if (g_shared_data->state == STATE_CONNECTED) {
						timestamp();

                        if (INT32_MIN != g_shared_data->keepalive_in_ms) {

                            if (g_shared_data->last_updated_timer_in_ms > a_action_ptr->action_argument.epalsed_time_in_ms)
                                g_shared_data->last_updated_timer_in_ms -= a_action_ptr->action_argument.epalsed_time_in_ms;
                            else
                                g_shared_data->last_updated_timer_in_ms = 0;
							
							printf("keepalive %i %i\n", a_action_ptr->action_argument.epalsed_time_in_ms, g_shared_data->last_updated_timer_in_ms);
							
                            if ( 0 >= g_shared_data->last_updated_timer_in_ms) {
                                status = mqtt_ping_req(g_shared_data->out_fptr);
								if (status != Successfull) {
									mqtt_printf("keep alive failed %u\n", status);
								} else {
									mqtt_printf("keepalive\n");
									g_shared_data->last_updated_timer_in_ms = g_shared_data->keepalive_in_ms;
								}
                            } else {
                                status = PingNotSend;
                            }
                        } else {
                            status = Successfull;
                        }
                    }
                } else {
                    mqtt_printf("%s %u Keepalive argument NULL %p\n", __FILE__, __LINE__, (void*)a_action_ptr); 
                }
                break;
            
            case ACTION_PARSE_INPUT_STREAM:
                status = mqtt_parse_input_stream(a_action_ptr->action_argument.input_stream_ptr->data, 
                                                 &(a_action_ptr->action_argument.input_stream_ptr->size_of_data));
                if (NULL != g_shared_data)
                     g_shared_data->last_updated_timer_in_ms = g_shared_data->keepalive_in_ms;
                break;

            default:
            
                mqtt_printf("%s %u Invalid MQTT command %u\n", __FILE__, __LINE__, (uint32_t)a_action);
                status = InvalidArgument;
                break;
        }

    return status;
}
