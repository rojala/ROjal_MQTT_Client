#include "mqtt.h"
#include "unity.h"

/****************************************************************************************
 * FIXED HEADER TESTS                                                                   *
 * CONNACK                                                                              *
 ****************************************************************************************/
void test_decode_variable_header_connack()
{
    /* Thest that fixed header with all zeros is really 0x0000 */
    uint8_t input[] = {0x00, 0x01, 0x00};
    uint8_t state = 2;
    TEST_ASSERT_EQUAL_PTR(input + 2, decode_variable_header_conack(input, &state));
    TEST_ASSERT_EQUAL_UINT8(0x01, state);
    input[1] = 0xFF;
    TEST_ASSERT_EQUAL_PTR(input + 2, decode_variable_header_conack(input, &state));
    TEST_ASSERT_EQUAL_UINT8(0xFF, state);
}

void test_decode_variable_header_connect_zero()
{
    uint8_t output[10];
    uint8_t expected[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x00, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8(10, encode_variable_header_connect(output,
                                                               false,
                                                               false, 
                                                               QoS0,
                                                               false,
                                                               false,
                                                               false,
                                                               0));
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, output, 1, 10);
}

void test_decode_variable_header_connect_flag_user()
{
    uint8_t output[10];
    uint8_t expected[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x80, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8(10, encode_variable_header_connect(output,
                                                               false,
                                                               false, 
                                                               QoS0,
                                                               false,
                                                               false,
                                                               true,
                                                               0));
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, output, 1, 10);
}

void test_decode_variable_header_connect_flag_pass()
{
    uint8_t output[10];
    uint8_t expected[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x40, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8(10, encode_variable_header_connect(output,
                                                               false,
                                                               false, 
                                                               QoS0,
                                                               false,
                                                               true,
                                                               false,
                                                               0));
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, output, 1, 10);
}

void test_decode_variable_header_connect_flag_perm_lwt()
{
    uint8_t output[10];
    uint8_t expected[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x20, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8(10, encode_variable_header_connect(output,
                                                               false,
                                                               false, 
                                                               QoS0,
                                                               true,
                                                               false,
                                                               false,
                                                               0));
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, output, 1, 10);
}

void test_decode_variable_header_connect_flag_qos2()
{
    uint8_t output[10];
    uint8_t expected[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x10, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8(10, encode_variable_header_connect(output,
                                                               false,
                                                               false, 
                                                               QoS2,
                                                               false,
                                                               false,
                                                               false,
                                                               0));
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, output, 1, 10);
}

void test_decode_variable_header_connect_flag_qos1()
{
    uint8_t output[10];
    uint8_t expected[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x08, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8(10, encode_variable_header_connect(output,
                                                               false,
                                                               false, 
                                                               QoS1,
                                                               false,
                                                               false,
                                                               false,
                                                               0));
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, output, 1, 10);
}

void test_decode_variable_header_connect_flag_lwt()
{
    uint8_t output[10];
    uint8_t expected[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x04, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8(10, encode_variable_header_connect(output,
                                                               false,
                                                               true, 
                                                               QoS0,
                                                               false,
                                                               false,
                                                               false,
                                                               0));
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, output, 1, 10);
}

void test_decode_variable_header_connect_flag_clean_session()
{
    uint8_t output[10];
    uint8_t expected[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x02, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8(10, encode_variable_header_connect(output,
                                                               true,
                                                               false, 
                                                               QoS0,
                                                               false,
                                                               false,
                                                               false,
                                                               0));
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, output, 1, 10);
}

/****************************************************************************************
 * TEST main                                                                            *
 ****************************************************************************************/
int main(void)
{  
    UnityBegin("Decode variable header");
    unsigned int tCntr = 1;

    /* CONNACK */
    RUN_TEST(test_decode_variable_header_connack,                    tCntr++);

    /* Connect request frame */
    RUN_TEST(test_decode_variable_header_connect_zero,               tCntr++);
    RUN_TEST(test_decode_variable_header_connect_flag_user,          tCntr++);
    RUN_TEST(test_decode_variable_header_connect_flag_pass,          tCntr++);
    RUN_TEST(test_decode_variable_header_connect_flag_perm_lwt,      tCntr++);
    RUN_TEST(test_decode_variable_header_connect_flag_qos2,          tCntr++);
    RUN_TEST(test_decode_variable_header_connect_flag_qos1,          tCntr++);
    RUN_TEST(test_decode_variable_header_connect_flag_lwt,           tCntr++);
    RUN_TEST(test_decode_variable_header_connect_flag_clean_session, tCntr++);

    return (UnityEnd());
}
