
// kirjastot, samat kuin alkuperäisessä main.c:ssä, lisätty stdbool.h ja math.h
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

#include <stdbool.h>

#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX 1

// laitteen asento
typedef enum {
    POSITION_NONE, // nollatila -> ei lähetetä
    POSITION_SIDE, // laite kyljellään -> '-'
    POSITION_DOWN // laite pöydällä -> '.'
} position_t;



// nappi
typedef enum {
    EVENT_BUTTON1,
    EVENT_BUTTON2
} button_event_t;

// tilakoneen esittely

enum state {WAITING = 1, DATA_READY};
enum state programState = WAITING;

uint32_t gyro_sensor;

// 
static void sensor_task(void * arg) {
    (void)arg;

    // initializing buzzer for testing
    init_ICM42670();

    while(1){
        if(programState == WAITING) {
            gyro_sensor = ICM42670_read_sensor_data();
            programState = DATA_READY;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


// haetaan alkuperäisestä main.c:stä 

static void btn_fxn(uint gpio, uint32_t eventMask) {
    
    gpio_get(SW1_PIN);
    
    toggle_led();
}

static void print_task(void *arg){
    (void)arg;
    
    while(1){
        if (programState == DATA_READY) {
            printf("ASENTO: %lu \n",gyro_sensor);
            programState = WAITING;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
static void usbTask(void *arg) {
    (void)arg;
    while (1) {
        tud_task();              // With FreeRTOS wait for events
                                 // Do not add vTaskDelay. 
    }
}

int main() {

    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    init_sw1();
    gpio_set_dir(BUTTON1, GPIO_IN);
    
    init_led();
    gpio_set_dir(LED1, GPIO_OUT);
    
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL, true, btn_fxn);

    
    
    TaskHandle_t hSensorTask, hPrintTask, hUSB = NULL;

    xTaskCreate(usbTask, "usb", 2048, NULL, 3, &hUSB);
    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(hUSB, 1u << 0);
    #endif


    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(sensor_task, // (en) Task function
                "sensor",                        // (en) Name of the task 
                DEFAULT_STACK_SIZE,              // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                            // (en) Arguments of the task 
                2,                               // (en) Priority of this task
                &hSensorTask);                   // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Sensor task creation failed\n");
        return 0;
    }
    result = xTaskCreate(print_task,  // (en) Task function
                "print",              // (en) Name of the task 
                DEFAULT_STACK_SIZE,   // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                 // (en) Arguments of the task 
                2,                    // (en) Priority of this task
                &hPrintTask);         // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();
    
    // Never reach this line.
    return 0;
}

