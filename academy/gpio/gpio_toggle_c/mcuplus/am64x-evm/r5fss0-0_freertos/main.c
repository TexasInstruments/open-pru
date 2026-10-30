/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
 */

#include <stdlib.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_board_config.h"
#include "FreeRTOS.h"
#include "task.h"

#define MAIN_TASK_PRI  (configMAX_PRIORITIES-1)

#define MAIN_TASK_SIZE (16384U/sizeof(configSTACK_DEPTH_TYPE))
StackType_t gMainTaskStack[MAIN_TASK_SIZE] __attribute__((aligned(32)));

StaticTask_t gMainTaskObj;
TaskHandle_t gMainTask;

void gpio_toggle_c_main(void *args);

void freertos_main(void *args)
{
    gpio_toggle_c_main(NULL);

    vTaskDelete(NULL);
}


int main(void)
{
    /* init SOC specific modules */
    System_init();
    Board_init();

    /* This task is created at highest priority, it should create more tasks and then delete itself */
    gMainTask = xTaskCreateStatic( freertos_main,   /* Pointer to the function that implements the task. */
                                  "freertos_main", /* Text name for the task.  This is to facilitate debugging only. */
                                  MAIN_TASK_SIZE,  /* Stack depth in units of StackType_t typically uint32_t on 32b CPUs */
                                  NULL,            /* We are not using the task parameter. */
                                  MAIN_TASK_PRI,   /* task priority, 0 is lowest priority, configMAX_PRIORITIES-1 is highest */
                                  gMainTaskStack,  /* pointer to stack base */
                                  &gMainTaskObj ); /* pointer to statically allocated task object memory */
    configASSERT(gMainTask != NULL);

    /* Start the scheduler to start the tasks executing. */
    vTaskStartScheduler();

    /* The following line should never be reached because vTaskStartScheduler()
    will only return if there was not enough FreeRTOS heap memory available to
    create the Idle and (if configured) Timer tasks.  Heap management, and
    techniques for trapping heap exhaustion, are described in the book text. */
    DebugP_assertNoLog(0);

    return 0;
}
