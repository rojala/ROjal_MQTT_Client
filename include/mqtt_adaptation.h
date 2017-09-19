#ifndef MQTT_ADAPTATION_H
#define MQTT_ADAPTATION_H

#ifdef BUILD_DEFAULT_C_LIBS
#include <unistd.h>  // sleep
#include <stdio.h>   // printf
#include <string.h>  // memcpy, strlen

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

#endif /* BUILD_DEFAULT_C_LIBS */

#ifdef BUILD_FREERTOS

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memcpy, strlen

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "FreeRTOSIPConfig.h"

#define DEBUG

/**
 * mqtt_printf
 *
 * printf function to print out debugging data.
 *
 */
#define mqtt_printf vLoggingPrintf

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


#define mqtt_sleep(x) vTaskDelay(x/portTICK_PERIOD_MS)

#define mqtt_strlen strlen

#endif /* BUILD_FREERTOS */

#endif
