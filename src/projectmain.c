
// kirjastot, samat kuin alkuperäisessä main.c:ssä, lisätty stdbool.h ja math.h
#include <stdio.h>
#include <math.h>
#include <stdint.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#include "tkjhat/sdk.h"

#include <stdbool.h>


#define DEFAULT_STACK_SIZE 2048
#define BUTTON_DEBOUNCE_MS 750 //nappi ei ota kahta kertaa painalluksesta ku tuo pingi
#define RX_BUFFER_SIZE 64

// laitteen asento
typedef enum {
    POSITION_UP, // laite ylöspäin -> '.'
    POSITION_SIDE, // laite kyljellään -> '-'
    POSITION_DOWN // nollatila/pöydällä -> ei lähetetä
} position_t;


// tilakoneen esittely

enum state {
    WAITING = 1,        //WAITING: lähetystila, odotetaan seuraavaa anturilukemaa,
    DATA_READY,        //DATA_READY: anturi on luettu ja asento on valmis lähetettäväksi   
    RECEIVING           //RECEIVING: vastaanottotila, kuunnellaan työasemaa
};
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



static void sensor_task(void * arg) {  //Tunnistaa laitteen asennon
    (void)arg;

    ICM42670_DATA arvot;

    init_ICM42670();
    ICM42670_start_with_default_values();

    while(1){
        if(programState == WAITING) {
            /*
            if(ICM42670_read_sensor_data(&arvot.ax, &arvot.ay, &arvot.az, &arvot.gx, &arvot.gy, &arvot.gz, &arvot.t) == 0) {
                printf("ax=%.2f, ay=%.2f, az=%.2f\n", arvot.ax, arvot.ay, arvot.az);
            } else {
                printf("Epäonnistui\n");
            }
                toimiii tällä
            */

            
                if (ICM42670_read_sensor_data(&arvot.ax, &arvot.ay, &arvot.az, &arvot.gx, &arvot.gy, &arvot.gz, &arvot.t) == 0) {
                if(fabsf(arvot.ay) > 0.8f)       asento = POSITION_UP;
                else if (fabsf(arvot.ax) > 0.8f) asento = POSITION_SIDE;
                else                             asento = POSITION_DOWN;
                programState = DATA_READY;    
            } 
            

        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


// haetaan alkuperäisestä main.c:stä 

static volatile bool vali_lyonti = false;
static volatile bool tilan_vaihto = false;

static void btn_fxn(uint gpio, uint32_t eventMask) {  
    (void)eventMask;
    static uint32_t last_ms1 = 0;
    static uint32_t last_ms2 = 0;
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    if (gpio == BUTTON1) {
        if ((now_ms - last_ms1) < BUTTON_DEBOUNCE_MS) return;
        last_ms1 = now_ms;
        vali_lyonti = true;
    }
    else if (gpio == BUTTON2) {
        if ((now_ms - last_ms2) < BUTTON_DEBOUNCE_MS) return;
        last_ms2 = now_ms;
        tilan_vaihto = true;
    }
}

static void print_task(void *arg){
    (void)arg;

    while(1){

        if (programState == DATA_READY) {  
            if (asento == POSITION_UP){
                printf(".");
            }
            else if(asento == POSITION_SIDE){
                printf("-");
            }
            programState = WAITING;
        }

        if (vali_lyonti) {
            vali_lyonti = false;
            if (programState == WAITING) {
                printf("␣");
            }
        }

        if (tilan_vaihto) {
            tilan_vaihto = false;
            if (programState == WAITING) {
                printf("\n");               // viestin lopetus
                clear_display();
                write_text("Lahetetty");
                programState = RECEIVING;
            }
            else if (programState == RECEIVING) {
                programState = WAITING;
                clear_display();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void receive_task(void *arg) {
    (void)arg;
    char viesti[RX_BUFFER_SIZE];
    int pituus = 0;

    while (1) {
        if (programState == RECEIVING) {
            int c = getchar_timeout_us(0);
            while (c != PICO_ERROR_TIMEOUT) {
                if (c == '\n' || c == '\r') {
                    if (pituus > 0) {               // ohitetaan tyhjä rivi (\r\n)
                        viesti[pituus] = '\0';
                        clear_display();
                        write_text(viesti);

                        gpio_put(LED1, 1);          // Viestin vastaanottamisen yhteydessä väläytetään lediä
                        vTaskDelay(pdMS_TO_TICKS(200));
                        gpio_put(LED1, 0);
                        
                        pituus = 0;
                    }
                }
                else if ((c == '.' || c == '-' || c == ' ') && pituus < RX_BUFFER_SIZE - 1) {
                    viesti[pituus++] = (char)c;
                }   
            
                c = getchar_timeout_us(0);
            }
        }
        else {
            // lähetystilassa: heitetään työaseman merkit pois ja aloitetaan tyhjästä
            while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT) {}
            pituus = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}




int main() {

    stdio_init_all();

    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    init_sw1();
    init_sw2();
    
    init_led();
    init_display();
 
    
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL, true, btn_fxn);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_FALL, true);

    
    TaskHandle_t hSensorTask, hPrintTask = NULL;
    

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

        result = xTaskCreate(receive_task,
                    "receive",
                    DEFAULT_STACK_SIZE,
                    NULL,
                    2,
                    NULL);

    if(result != pdPASS) {
        printf("Receive task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();
    
    // Never reach this line.
    return 0;
}

