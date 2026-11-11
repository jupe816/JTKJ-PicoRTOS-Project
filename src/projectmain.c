
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
#include <tusb.h>

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
volatile enum state programState = WAITING;
volatile position_t asento;

typedef struct {
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
    float t;
} ICM42670_DATA;

// ICM42670 sensor task
static void sensor_task(void * arg) {
    (void)arg;

    ICM42670_DATA arvot;

    init_ICM42670();

    while(1){
        if(programState == WAITING) {
            if (ICM42670_read_sensor_data(&arvot.ax, &arvot.ay, &arvot.az, &arvot.gx, &arvot.gy, &arvot.gz, &arvot.t) == 0) {
                if(fabsf(arvot.az) > 0.8f)       asento = POSITION_DOWN;
                else if (fabsf(arvot.ax) > 0.8f) asento = POSITION_SIDE;
                else                             asento = POSITION_NONE;
                programState = DATA_READY;    
            }
            

        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


// haetaan alkuperäisestä main.c:stä 

static void btn_fxn(uint gpio, uint32_t eventMask) {
    
    gpio_get(SW1_PIN);
    
}

static void print_task(void *arg){
    (void)arg;
    
    while(1){
        if (programState == DATA_READY) {
            if (asento == POSITION_DOWN){
                printf(".");
            }
            else if(asento == POSITION_SIDE){
                printf("-");
            }

            programState = WAITING;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*static void usbTask(void *arg) {
    (void)arg;
    while (1) {
        tud_task();              // With FreeRTOS wait for events
                                 // Do not add vTaskDelay. 
    }
}*/

int main() {

    stdio_init_all();

    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    init_sw1();
    gpio_set_dir(BUTTON1, GPIO_IN);
    
    init_led();
    gpio_set_dir(LED1, GPIO_OUT);
    
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL, true, btn_fxn);

    while(1){
        
    }
    TaskHandle_t hSensorTask, hPrintTask, hUSB = NULL;
    
    /*
    xTaskCreate(usbTask, "usb", 2048, NULL, 3, &hUSB);
    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(hUSB, 1u << 0);
    #endif
    */
    //tusb_init();
    //usb_serial_init();

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

